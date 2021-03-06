#ifndef _BEMRosetta_BEMRosetta_cl_FastOut_h_
#define _BEMRosetta_BEMRosetta_cl_FastOut_h_


class FastOut {
public:
	static Vector<String> GetFilesToLoad(String path);
	static String GetFileToLoad(String fileName);
	bool Load0(String fileName);
	bool Save(String fileName);
	
	void Clear();
	int GetCol(String param);
	int FindCol(String param);
	Upp::Vector<int> FindColMatch(String param);
	
	const String &GetParameter(int id) const	{return parameters[id];}
	const String &GetUnit(int id) const			{return units[id];}
	int GetColumnCount() const					{return parameters.GetCount();}
	
	SortedIndex<String> GetParameterList(String filter = ""); 
	SortedIndex<String> GetUnitList(String filter = "");
	SortedVectorMap<String, String> GetList(String filterParam = "", String filterUnits = "");
		
	double GetVal(double time, int col);
	inline double GetVal(int idtime, int col) 	{return dataOut[col][idtime];}
	int GetIdTime(double time);
	double GetDuration()	{return dataOut[0][GetCount()-1];}
	int GetCount()			{return dataOut[0].GetCount();}			
	bool IsEmpty()			{return dataOut.IsEmpty();}	
	
	int ColFairlead(int i) 	{return GetCol(Format("T[%d]", i-1));}
		
	int GetNumFairlead() {
		for (int i = 0; i < 100; ++i) {
			if (ColFairlead(i) < 0) 
				return i;
		}
		return Null;
	}
	
	void Serialize(Stream& s) {
        s % dataOut % parameters % units;
    }
	
	struct CalcParam {
		CalcParam(FastOut &_dataFast) : dataFast(&_dataFast)	{}
		virtual void Init() = 0;
		virtual double Calc(int idt) = 0;
		
	protected:
		FastOut *dataFast = 0;
	};
	
	struct CalcParams {
		String name, units;
		CalcParam *calc;
	};
	
	void AddParam(String name, String units, CalcParam &calc) {
		CalcParams &c = calcParams.Add();
		c.name = name;
		c.units = units;
		c.calc = &calc;
	}

	Upp::Vector<String> parameters, units;
	Upp::Vector<Upp::Vector <double> > dataOut;
	Upp::Array<CalcParams> calcParams;

private:
	bool LoadOut(String fileName);
	bool LoadOutb(String fileName);
	void AfterLoad();

	String lastFile;
	Time lastTime;
};


#endif
