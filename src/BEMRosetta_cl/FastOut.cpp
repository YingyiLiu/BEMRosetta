#include "BEMRosetta.h"

#include <Core/Core.h>
#include <SysInfo/SysInfo.h>
#include <Surface/Surface.h>

using namespace Upp;

#include "FastOut.h"

static int IsTabSpaceRet(int c) {
	if (c == '\t' || c == ' ' || c == '\r' || c == '\n')
		return true; 
	return false;
} 

Vector<String> FastOut::GetFilesToLoad(String path) {
	Vector<String> ret;
	
	if (TrimBoth(path).IsEmpty())
		return ret;
	
	if (!DirectoryExists(path)) {
		if (FileExists(path))
			ret << GetFileToLoad(path);
		return ret;
	} 
	FindFile ff(AppendFileName(path, "*.out*"));
	int64 sz = -1;
	String fileName;
	while (ff) {
		if (ff.IsFile()) { 
			String name = GetFileToLoad(ff.GetPath());
			if (!IsNull(name)) {
				if (GetFileExt(name) == ".outb") {
					ret << name;
					return ret;
				}
				int64 nsz = GetFileLength(name);
				if (nsz > sz) {
					sz = nsz;
					fileName = name;
				}
			}
		}
		ff.Next();
	}
	if (!fileName.IsEmpty()) {
		ret << fileName;
		return ret;
	}
	ff.Search(AppendFileName(path, "*.*"));
	while (ff) {
		if (ff.IsFolder())
			ret.Append(GetFilesToLoad(ff.GetPath()));
		ff.Next();
	}
	return ret;
}

String FastOut::GetFileToLoad(String fileName) {
	if (TrimBoth(fileName).IsEmpty())
		return Null;
		
	String strOut = ForceExt(fileName, ".out");
	String strOutB = ForceExt(fileName, ".outb");

	bool exOut = FileExists(strOut);
	bool exOutB = FileExists(strOutB);

	if (exOut && !exOutB) 
		return strOut;
	else if (!exOut && exOutB) 
		return strOutB;
	else if (exOut && exOutB) {
		Time tOut = FileGetTime(strOut);
		Time tOutB = FileGetTime(strOutB);
		if (abs(tOut - tOutB) < 5 || tOutB > tOut) 
			return strOutB;
		else 
			return strOut;
	}
	return Null;
}

bool FastOut::Load0(String fileName) {	
	Time actualTime = FileGetTime(fileName);
	if (lastFile == fileName && actualTime - lastTime < 5) // Only loads if file is 5 sec older
		return true;
	 
	lastFile = fileName;
	lastTime = actualTime;
	
	String ext = GetFileExt(fileName);
	bool ret = false;
	if (ext == ".out")
		ret = LoadOut(fileName);
	else if (ext == ".outb")
		ret = LoadOutb(fileName);
			
	if (ret)
		AfterLoad();
	return ret;
}
	
bool FastOut::LoadOut(String fileName) {
	String raw = LoadFileBOM(fileName);
	if (raw.IsEmpty()) 
		throw Exc(Format("Problem reading '%s'", fileName)); 
	
	Clear();
	bool begin = false;
	int row = -1;
	int pos = 0, npos = 0;
	int numCol;
	while (npos >= 0) {
		npos = raw.FindAfter("\n", pos);
		String line = raw.Mid(pos, npos-pos);
		Vector<String> fields = Split(line, IsTabSpaceRet, true);
		
		if (!begin) {
			if (!fields.IsEmpty() && fields[0] == "Time") {
				for (int c = 0; c < fields.GetCount(); ++c) 
					parameters << fields[c];
				pos = npos;
				npos = raw.FindAfter("\n", pos);
				line = raw.Mid(pos, npos-pos);
				Vector<String> fields = Split(line, IsTabSpaceRet, true);
				for (int c = 0; c < fields.GetCount(); ++c) 
					units << Replace(Replace(fields[c], "(", ""), ")", "");
				begin = true;
				if (parameters.GetCount() != units.GetCount()) 
					throw Exc("Number of parameters and units do not match");

				numCol = parameters.GetCount();
				dataOut.SetCount(numCol+calcParams.GetCount());
			}
		} else {
			row++;
			if (fields.IsEmpty())
				return true;
			if (fields.GetCount() != numCol) 
				throw Exc(Format("Number of values (%d) and parameters (%d) do not match in row %d", fields.GetCount(), numCol, row));

			for (int c = 0; c < fields.GetCount(); ++c) 
				dataOut[c] << ScanDouble(fields[c]);
		}
		pos = npos;
	}
	if (dataOut.IsEmpty()) 
		throw Exc(Format("Problem reading '%s'", fileName)); 

	return true;
}

bool FastOut::LoadOutb(String fileName) {
	Clear();
	
	int LenName = 10;  							// number of characters per channel name
	int LenUnit = LenName;  					// number of characters per unit name

	enum FileFmt {WithTime = 1, WithoutTime, ChanLen};

	FileInBinary file(fileName);
	if (!file.IsOpen())
		return false;

	int16 FileID = file.ReadB<int16,2>();
	int32 NumOutChans = file.ReadB<int32,4>();  // The number of output channels, INT(4)
    int32 NT = file.ReadB<int32,4>();			// The number of time steps, INT(4)

	double TimeScl, TimeOff, TimeOut1, TimeIncr;
    if (FileID == FileFmt::WithTime) {
        TimeScl = file.ReadB<double,8>(); 		// The time slopes for scaling, REAL(8)
        TimeOff = file.ReadB<double,8>();   	// The time offsets for scaling, REAL(8)
    } else {
        TimeOut1 = file.ReadB<double,8>();  	// The first time in the time series, REAL(8)
        TimeIncr = file.ReadB<double,8>();  	// The time increment, REAL(8)
    }

	Buffer<float> ColScl(NumOutChans), ColOff(NumOutChans);
    file.ReadB(ColScl, 4*NumOutChans);			// The channel slopes for scaling, REAL(4)
    file.ReadB(ColOff, 4*NumOutChans); 			// The channel offsets for scaling, REAL(4)

	int32 LenDesc = file.ReadB<int32,4>();		// The number of characters in the description string, INT(4)
    StringBuffer DescStrB(LenDesc);
    file.ReadB(DescStrB, LenDesc);  			// DescStr converted to ASCII
    String DescStr = DescStrB;
    
    if (FileID == FileFmt::ChanLen) {
        LenName = 15;
        LenUnit = LenName;
    }

	parameters.SetCount(NumOutChans+1);     	// initialize the ChanName cell array
	parameters[0] = "Time";
	Buffer<char> name(LenName);
    for (int iChan = 0; iChan < NumOutChans+1; ++iChan) { 
        file.ReadB(name, LenName); 				// ChanName converted to numeric ASCII
        parameters[iChan] = TrimBoth(String(name, LenName));
    }
    
	units.SetCount(NumOutChans+1);          	// initialize the ChanName cell array
	units[0] = "s";
	Buffer<char> unit(LenUnit);
    for (int iChan = 0; iChan < NumOutChans+1; ++iChan) { 
        file.ReadB(unit, LenUnit); 				// ChanName converted to numeric ASCII
        units[iChan] = Replace(Replace(TrimBoth(String(unit, LenUnit)), "(", ""), ")", "");
    }  
    
    int nPts = NT*NumOutChans;           		// number of data points in the file   
    dataOut.SetCount(NumOutChans+1+calcParams.GetCount());
    for (int i = 0; i < dataOut.GetCount(); ++i)
        dataOut[i].SetCount(NT);
    
    Buffer<int32> bufferTime;
    if (FileID == FileFmt::WithTime) {
        bufferTime.Alloc(NT);
        file.ReadB(bufferTime, 4*NT); 			// read the time data
    }
    
    Buffer<int16> bufferData(nPts);
    file.ReadB(bufferData, 2*nPts); 				// read the channel data
    int ip = 0;
    for (int idt = 0; idt < NT; ++idt) 
    	for (int i = 1; i < NumOutChans+1; ++i) {
	    	double off = ColOff[i-1];
	    	double scl = ColScl[i-1];
	        dataOut[i][idt] = (bufferData[ip] - off)/scl;
	        ip++;
	}    
    
    if (FileID == FileFmt::WithTime) {
        for (int idt = 0; idt < NT; ++idt)
            dataOut[0][idt] = ( bufferTime[idt] - TimeOff)/ TimeScl;
    } else {
        for (int idt = 0; idt < NT; ++idt)
            dataOut[0][idt] = TimeOut1 + TimeIncr*idt;
    }

	return true;
}

void FastOut::AfterLoad() {
	for (CalcParams &c : calcParams) {
		parameters << c.name;
		units << c.units;
		int id = parameters.GetCount()-1;
		c.calc->Init();
		for (int idt = 0; idt < dataOut[id].GetCount(); ++idt)
			dataOut[id][idt] = c.calc->Calc(idt);
	}
}

void FastOut::Clear() {
	parameters.Clear();	
	units.Clear();		
	dataOut.Clear();
}

int FastOut::FindCol(String param) {
	param = ToLower(param);
	for (int c = 0; c < parameters.GetCount(); ++c) {
		if (ToLower(parameters[c]) == param)
			return c;
	}
	return Null;
}

Vector<int> FastOut::FindColMatch(String param) {
	param = ToLower(param);
	Vector<int> ret;
	for (int c = 0; c < parameters.GetCount(); ++c) {
		if (PatternMatch(param, ToLower(parameters[c])))
			ret << c;
	}
	return ret;
}

int FastOut::GetCol(String param) {
	int ret = FindCol(param);
	if (IsNull(ret))
		throw Exc(Format("Parameter '%s' not found", param));	
	return ret;
}

double FastOut::GetVal(double time, int col) {
	int idtime = GetIdTime(time);
	if (IsNull(idtime))
		return Null;
	return GetVal(idtime, col);
}

int FastOut::GetIdTime(double time) {
	if (time < 0)
		return Null;	
	for (int r = 0; r < dataOut[0].GetCount(); ++r) {
		if (dataOut[0][r] >= time)
			return r;
	}
	return Null;
}

SortedIndex<String> FastOut::GetParameterList(String filter) {
	SortedIndex<String> list;
	
	if (filter.IsEmpty()) {
		for (int i = 0; i < GetColumnCount(); ++i) 
			list.FindAdd(GetParameter(i));
	} else {
		filter = "*" + ToLower(filter) + "*";
		for (int i = 0; i < GetColumnCount(); ++i) {
			String str = ToLower(GetParameter(i));
			if (PatternMatch(filter, str))
				list.FindAdd(GetParameter(i));
		}
	}
	//SortIndex(list);
	return list;//.PickKeys();
}

SortedIndex<String> FastOut::GetUnitList(String filter) {
	SortedIndex<String> list;
	
	if (filter.IsEmpty()) {
		for (int i = 0; i < GetColumnCount(); ++i) 
			list.FindAdd(GetUnit(i));
	} else {
		filter = "*" + ToLower(filter) + '*';
		for (int i = 0; i < GetColumnCount(); ++i) {
			String str = ToLower(GetUnit(i));
			if (PatternMatch(filter, str))
				list.FindAdd(GetUnit(i));
		}
	}
	//SortIndex(list);
	return list;//.PickKeys();
}

SortedVectorMap<String, String> FastOut::GetList(String filterParam, String filterUnits) {
	SortedVectorMap<String, String> list;
	
	if (filterParam.IsEmpty() && filterUnits.IsEmpty()) {
		for (int i = 0; i < GetColumnCount(); ++i) 
			list.Add(GetParameter(i), GetUnit(i));
	} else {
		filterParam = "*" + ToLower(filterParam) + '*';
		filterUnits = "*" + ToLower(filterUnits) + '*';
		for (int i = 0; i < GetColumnCount(); ++i) {
			String strParams = ToLower(GetParameter(i));
			String strUnits = ToLower(GetUnit(i));
			if (PatternMatch(filterParam, strParams) && 
				PatternMatch(filterUnits, strUnits))
				list.Add(GetParameter(i), GetUnit(i));
		}
	}
	return list;
}