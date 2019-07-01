#include "BEMRosetta.h"


bool Wamit::Load(String file, double rho) {
	hd().code = Hydro::WAMIT;
	hd().file = file;	
	hd().name = GetFileTitle(file);
	
	hd().g = Null;
	hd().rho = rho;
	hd().len = Null;
	
	hd().Nb = Null;
	
	try {
		if (GetFileExt(file) == ".out") {
			hd().Print("\n\n" + Format(t_("Loading out file '%s'"), file));
			if (!Load_out()) {
				hd().PrintWarning("\n" + Format(t_("File '%s' not found"), file));
				return false;
			}
			String fileSC = ForceExt(file, ".3sc");
			hd().Print("\n- " + Format(t_("Scattering file '%s'"), GetFileName(fileSC)));
			if (!Load_Scattering(fileSC))
				hd().PrintWarning(x_(": **") + t_("Not found") + "**");
			String fileFK = ForceExt(file, ".3fk");
			hd().Print("\n- " + Format(t_("Froude-Krylov file '%s'"), GetFileName(fileFK)));
			if (!Load_FK(fileFK))
				hd().PrintWarning(x_(": **") + t_("Not found") + "**");
		} else if (x_(".1.3.hst").Find(GetFileExt(file)) >= 0) {
			String file1 = ForceExt(file, ".1");
			hd().Print("\n- " + Format(t_("Hydrodynamic coefficients A and B .1 file '%s'"), GetFileName(file1)));
			if (!Load_1(file1))
				hd().PrintWarning(x_(": **") + t_("Not found") + "**");
			
			String file3 = ForceExt(file, ".3");
			hd().Print("\n- " + Format(t_("Diffraction exciting .3 file '%s'"), GetFileName(file3)));
			if (!Load_3(file3))
				hd().PrintWarning(x_(": **") + t_("Not found") + "**");
			
			String fileHST = ForceExt(file, ".hst");
			hd().Print("\n- " + Format(t_("Hydrostatic restoring file '%s'"), GetFileName(fileHST)));
			if (!Load_hst(fileHST))
				hd().PrintWarning(x_(": **") + t_("Not found") + "**");
		}
		String fileRAO = ForceExt(file, ".4");
		hd().Print("\n- " + Format(t_("RAO file '%s'"), GetFileName(fileRAO)));
		try {
			if (!Load_4(fileRAO))
				hd().PrintWarning(x_(": **") + t_("Not found") + "**");
		} catch(Exc e) {
			hd().PrintError(Format("\n%s: %s", t_("Error"), e));
			hd().lastError = e;
		}
		if (IsNull(hd().Nb))
			return false;
		
		hd().dof.Clear();	hd().dof.SetCount(hd().Nb, 0);
		for (int i = 0; i < hd().Nb; ++i)
			hd().dof[i] = 6;
	} catch (Exc e) {
		hd().PrintError(Format("\n%s: %s", t_("Error"), e));
		hd().lastError = e;
		return false;
	}
	
	return true;
}

void Wamit::Save(String file) {
	try {
		if (hd().IsLoadedA() && hd().IsLoadedB()) {
			String file1 = ForceExt(file, ".1");
			hd().Print("\n- " + Format(t_("Hydrodynamic coefficients A and B file '%s'"), GetFileName(file1)));
			Save_1(file1);
		}
		
		if (hd().IsLoadedFex()) {
			String file3 = ForceExt(file, ".3");
			hd().Print("\n- " + Format(t_("Diffraction exciting file '%s'"), GetFileName(file3)));
			Save_3(file3);
		}
		if (hd().IsLoadedC()) {
			String fileHST = ForceExt(file, ".hst");
			hd().Print("\n- " + Format(t_("Hydrostatic restoring file '%s'"), GetFileName(fileHST)));
			Save_hst(fileHST);
		}
		if (hd().IsLoadedRAO()) {
			String fileRAO = ForceExt(file, ".4");
			hd().Print("\n- " + Format(t_("RAO file '%s'"), GetFileName(fileRAO)));
			Save_4(fileRAO);
		}
	} catch (Exc e) {
		hd().PrintError(Format("\n%s: %s", t_("Error"), e));
		hd().lastError = e;
	}
}


bool Wamit::Load_out() {
	hd().Nb = 0;
	hd().Nf = 0;
	hd().Nh = 0;
	int pos;
	int ibody = -1;
	hd().dimen = false;
	
	FileInLine in(hd().file);
	if (!in.IsOpen())
		return false;
	String line;
	FieldSplit f(in);
	
	hd().names.Clear();
	while(!in.IsEof()) {
		line = in.GetLine();
		f.Load(line);
		if (line.Find("N=") >= 0 && line.Find("Body number:") < 0) {
			hd().Nb++;
			hd().names << GetFileTitle(f.GetText(2));
		} else if ((pos = line.FindAfter("Input from Geometric Data File:")) >= 0) {
			hd().Nb = 1;
			hd().names << GetFileTitle(TrimBoth(line.Mid(pos)));
		} else if (line.Find("POTEN run date and starting time:") >= 0) {
			hd().cg.setConstant(3, hd().Nb, Null);
			hd().cb.setConstant(3, hd().Nb, Null);
			hd().Vo.SetCount(hd().Nb, Null);
			hd().C.SetCount(hd().Nb);
		} else if (line.Find("Gravity:") >= 0) {
			hd().g = f.GetDouble(1);
			hd().len = f.GetDouble(4);
		} else if (line.Find("Water depth:") >= 0) {
			if (ToLower(f.GetText(2)) == "infinite")
				hd().h = -1;
			else {
				hd().h = f.GetDouble(2);
				if (hd().h < 0)
					throw(Exc(t_("Water depth has to be positive")));
			}
			if (line.Find("Water density:") >= 0) 
				hd().rho = f.GetDouble(5);			
		} else if (line.Find("XBODY =") >= 0) {
			ibody++;
			hd().cg(0, ibody) = f.GetDouble(2);
			hd().cg(1, ibody) = f.GetDouble(5);
			hd().cg(2, ibody) = f.GetDouble(8);
		} else if ((pos = line.FindAfter("Volumes (VOLX,VOLY,VOLZ):")) >= 0) 
			hd().Vo[ibody] = ScanDouble(line.Mid(pos));
		else if (line.Find("Center of Buoyancy (Xb,Yb,Zb):") >= 0) {
			hd().cb(0, ibody) = f.GetDouble(4) + hd().cg(0, ibody);
			hd().cb(1, ibody) = f.GetDouble(5) + hd().cg(1, ibody);
			hd().cb(2, ibody) = f.GetDouble(6) + hd().cg(2, ibody);
		} else if (line.Find("Hydrostatic and gravitational") >= 0) {
			hd().C[ibody].setConstant(6, 6, 0);
			f.LoadWamitJoinedFields(in.GetLine());
			hd().C[ibody](2, 2) = f.GetDouble(1);
			hd().C[ibody](2, 3) = hd().C[ibody](3, 2) = f.GetDouble(2);
			hd().C[ibody](2, 4) = hd().C[ibody](4, 2) = f.GetDouble(3);
			f.LoadWamitJoinedFields(in.GetLine());
			hd().C[ibody](3, 3) = f.GetDouble(1);
			hd().C[ibody](3, 4) = hd().C[ibody](4, 3) = f.GetDouble(2);
			hd().C[ibody](3, 5) = hd().C[ibody](5, 3) = f.GetDouble(3);
			f.LoadWamitJoinedFields(in.GetLine());
			hd().C[ibody](4, 4) = f.GetDouble(1);
			hd().C[ibody](4, 5) = hd().C[ibody](5, 4) = f.GetDouble(2);
		} else if (line.Find("Output from  WAMIT") >= 0) {
			hd().head.Clear();
			FileInLine::Pos fpos = in.GetPos();
			
			bool foundNh = false;
			while (!in.IsEof()) {
				line = in.GetLine();
				if (line.Find("Wave period (sec)") >= 0) {
					++hd().Nf;
					if (hd().Nh > 0 && !foundNh)
						foundNh = true;
				} else if (!foundNh) {
					if (hd().Nh > 0 && (line.Find("*********************") >= 0 ||
								   line.Find("FORCES AND MOMENTS") >= 0)) 
						foundNh = true;
					else if (line.Find("Wave Heading (deg) :") >= 0) {
						f.Load(line);
						hd().head << f.GetDouble(4);
						++hd().Nh;
					}
				}
			}
			if (hd().Nb == 0 || hd().Nh == 0 || hd().Nf == 0)
				throw Exc(Format(t_("Wrong format in Wamit file '%s'"), hd().file));
		
			hd().T.SetCount(hd().Nf);
			hd().w.SetCount(hd().Nf);
			
			in.Seek(fpos);
			while (in.GetLine().Find("Wave period = infinite") < 0 && !in.IsEof())
				; 
			if (!in.IsEof()) {
				hd().Aw0.setConstant(hd().Nb*6, hd().Nb*6, Null);
				Load_A(in, hd().Aw0);
			}
			in.Seek(fpos);
			while (in.GetLine().Find("Wave period = zero") < 0 && !in.IsEof())
				; 
			if (!in.IsEof()) {
				hd().Awinf.setConstant(hd().Nb*6, hd().Nb*6, Null);
				Load_A(in, hd().Awinf);
			}
			
			in.Seek(fpos);
			
			int ifr = -1;
			while (!in.IsEof()) {
				line = in.GetLine();
				while ((line = in.GetLine()).Find("Wave period (sec)") < 0 && !in.IsEof())
					; 
				if (in.IsEof())
					return true;
				
				f.Load(line);
				
				ifr++;
	            hd().T[ifr] = f.GetDouble(4);  			
	            hd().w[ifr] = fround(2*M_PI/hd().T[ifr], 8);
	            hd().dataFromW = false;
	            
	            bool nextFreq = false;
	            while (!in.IsEof() && !nextFreq) {
	            	line = in.GetLine();
	            	if (line.Find("ADDED-MASS AND DAMPING COEFFICIENTS") >= 0) {
	            		if (hd().A.IsEmpty()) {
							hd().A.SetCount(hd().Nf);
							hd().B.SetCount(hd().Nf);
						}
						in.GetLine(2);
						hd().A[ifr].setConstant(hd().Nb*6, hd().Nb*6, Null);
		            	hd().B[ifr].setConstant(hd().Nb*6, hd().Nb*6, Null);
		            
			            while (!in.IsEof()) {
							line = TrimBoth(in.GetLine());
							if (line.IsEmpty())
			                	break;
							f.Load(line);
							int i = f.GetInt(0) - 1;
							int j = f.GetInt(1) - 1;
							double Aij = f.GetDouble(2);
							double Bij = f.GetDouble(3);
							hd().A[ifr](i, j) = Aij;
							hd().B[ifr](i, j) = Bij;
						}
						hd().GetBodyDOF();
	            	} else if (line.Find("DIFFRACTION EXCITING FORCES AND MOMENTS") >= 0) {
						if (hd().ex.ma.IsEmpty()) 
							hd().Initialize_Forces(hd().ex);
						
						int ih = 0;
						while (!in.IsEof()) {		
							line = in.GetLine();
							if (line.Find("Wave Heading (deg) :") >= 0) {
								in.GetLine(3); 
								//int idof = 0;
								while (!TrimBoth(line = in.GetLine()).IsEmpty()) {
									f.Load(line);
									double ma = f.GetDouble(1);
									double ph = f.GetDouble(2)*M_PI/180;
									double re = ma*cos(ph);
									double im = ma*sin(ph);
									int i = abs(f.GetInt(0)) - 1;
									hd().ex.ma[ih](ifr, i) = ma;	
									hd().ex.ph[ih](ifr, i) = ph;	
									hd().ex.re[ih](ifr, i) = re;	
									hd().ex.im[ih](ifr, i) = im;	
								}
								ih++;
							} else if (line.Find("RESPONSE AMPLITUDE OPERATORS") >= 0 ||
									   line.Find("SURGE, SWAY & YAW DRIFT FORCES") >= 0 ||
									   line.Find("SURGE, SWAY, HEAVE, ROLL, PITCH & YAW DRIFT FORCES") >= 0 ||
									   line.Find("HYDRODYNAMIC PRESSURE IN FLUID DOMAIN") >= 0 ||
									   line.Find("*************************************") >= 0) {
								nextFreq = true;
								break;
							}
						}
					}
				}
			}
		}
	}

	return true;
}


void Wamit::Load_A(FileInLine &in, MatrixXd &A) {
	in.GetLine(6);
	while (!in.IsEof()) {
		String line = TrimBoth(in.GetLine());
		if (line.IsEmpty())
           	break;
		FieldSplit f(in);
		f.Load(line);
		int i = f.GetInt(0) - 1;
		int j = f.GetInt(1) - 1;
		double Aij = f.GetDouble(2);
		A(i, j) = Aij;
	}
}

bool Wamit::Load_Scattering(String fileName) {
	FileInLine in(fileName);
	if (!in.IsOpen())
		return false;
	FieldSplit f(in);
	
	hd().Initialize_Forces(hd().sc);
	
	in.GetLine();
    for (int ifr = 0; ifr < hd().Nf; ++ifr) {
        for (int ih = 0; ih < hd().Nh; ++ih) {
            for (int k = 0; k < hd().Nb*6; ++k) {		// Number of non-zero dof
        		f.Load(in.GetLine());
        		
        		int i = f.GetInt(2) - 1;
                hd().sc.ma[ih](ifr, i) = f.GetDouble(3);
                hd().sc.ph[ih](ifr, i) = f.GetDouble(4)*M_PI/180;
                hd().sc.re[ih](ifr, i) = f.GetDouble(5);
                hd().sc.im[ih](ifr, i) = f.GetDouble(6);
            }
        }
    }
    return true;
}
		
bool Wamit::Load_FK(String fileName) {
	FileInLine in(fileName);
	if (!in.IsOpen())
		return false;
	FieldSplit f(in);
	
	hd().Initialize_Forces(hd().fk);
	
	in.GetLine();	
    for (int ifr = 0; ifr < hd().Nf; ++ifr) {
        for (int ih = 0; ih < hd().Nh; ++ih) {
            for (int k = 0; k < hd().Nb*6; ++k) {		// Number of non-zero dof
                f.Load(in.GetLine());
        		
        		int i = f.GetInt(2) - 1;
                hd().fk.ma[ih](ifr, i) = f.GetDouble(3);
                hd().fk.ph[ih](ifr, i) = f.GetDouble(4)*M_PI/180;
                hd().fk.re[ih](ifr, i) = f.GetDouble(5);
                hd().fk.im[ih](ifr, i) = f.GetDouble(6);
            }
        }
    }	
    return true;
}

bool Wamit::Load_1(String fileName) {
	hd().dimen = false;
	hd().len = 1;
	
	FileInLine in(fileName);
	if (!in.IsOpen())
		return false;
	FieldSplit f(in);
 
 	FileInLine::Pos fpos;
 	while (IsNull(ScanDouble(in.GetLine())) && !in.IsEof())
 		fpos = in.GetPos();
	if (in.IsEof())
		throw Exc("Error in file format");
	
	Vector<double> T; 	
    Vector<double> w; 
    
	in.Seek(fpos);
	
	int maxDof = 0;
	bool thereIsAw0 = false, thereIsAwinf = false; 
	while (!in.IsEof()) {
		f.Load(in.GetLine());
		double freq = f.GetDouble(0);
		if (freq < 0)
			thereIsAw0 = true;
		else if (freq == 0)
			thereIsAwinf = true;
		else
			FindAdd(w, freq);
		
		int dof = f.GetInt(1);
		if (dof > maxDof)
			maxDof = dof-1;
	}
	
	int Nb = 1 + int(maxDof/6);
	if (!IsNull(hd().Nb) && hd().Nb < Nb)
		throw Exc(Format(t_("Number of bodies loaded is lower than previous (%d != %d)"), hd().Nb, Nb));
	hd().Nb = Nb;	
	
	int Nf = w.GetCount();
	if (!IsNull(hd().Nf) && hd().Nf != Nf)
		throw Exc(Format(t_("Number of frequencies loaded is different than previous (%d != %d)"), hd().Nf, Nf));
	hd().Nf = Nf;
	
	if (hd().Nb == 0 || hd().Nf < 2)
		throw Exc(Format(t_("Wrong format in Wamit file '%s'"), hd().file));
	
	if (w[0] > w[1]) {
		hd().dataFromW = false;
		T = pick(w);
		w.SetCount(hd().Nf);	
	} else {
		hd().dataFromW = true;
		T.SetCount(hd().Nf);
	}
	
	hd().A.SetCount(hd().Nf);
	hd().B.SetCount(hd().Nf);	
	if (thereIsAw0)
		hd().Aw0.setConstant(hd().Nb*6, hd().Nb*6, Null);
	if (thereIsAwinf)
		hd().Awinf.setConstant(hd().Nb*6, hd().Nb*6, Null);

	for (int ifr = 0; ifr < hd().Nf; ++ifr) {
		if (hd().dataFromW)
			T[ifr] = fround(2*M_PI/w[ifr], 8);
		else
			w[ifr] = fround(2*M_PI/T[ifr], 8);
		hd().A[ifr].setConstant(hd().Nb*6, hd().Nb*6, Null);
	  	hd().B[ifr].setConstant(hd().Nb*6, hd().Nb*6, Null);
	}
	
	if (hd().w.IsEmpty()) {
		hd().w = pick(w);
		hd().T = pick(T);
	} else if (!Compare(hd().w, w, 0.001))
		throw Exc(Format(t_("Frequencies loaded are different than previous\nPrevious: %s\nSeries:   %s"), ToString(hd().w), ToString(w)));
	else if (!Compare(hd().T, T, 0.001))
		throw Exc(Format(t_("Periods loaded are different than previous\nPrevious: %s\nSeries:   %s"), ToString(hd().T), ToString(T)));
				
	in.Seek(fpos);
	
	while (!in.IsEof()) {
		f.Load(in.GetLine());
		double freq = f.GetDouble(0);
 		int i = f.GetInt(1) - 1;
 		int j = f.GetInt(2) - 1;
 		double Aij = f.GetDouble(3);
 		
 		if ((freq < 0 && hd().dataFromW) || (freq == 0 && !hd().dataFromW))
			hd().Awinf(i, j) = Aij;
		else if (freq <= 0)
			hd().Aw0(i, j) = Aij;
		else {
			int ifr;
			if (hd().dataFromW)
				ifr = FindIndexRatio(hd().w, freq, 0.001);
			else
				ifr = FindIndexRatio(hd().T, freq, 0.001);
			if (ifr < 0)
				throw Exc(Format("Unknown frequency %f", freq));
		
		  	hd().A[ifr](i, j) = Aij;    
		  	hd().B[ifr](i, j) = f.GetDouble(4);   	
		}
	}		
	return true;	
}
 
bool Wamit::Load_3(String fileName) {
	hd().dimen = false;
	hd().len = 1;
	
	FileInLine in(fileName);
	if (!in.IsOpen())
		return false;
	FieldSplit f(in);
 
 	FileInLine::Pos fpos;
 	while (IsNull(ScanDouble(in.GetLine())) && !in.IsEof())
 		fpos = in.GetPos();
	if (in.IsEof())
		throw Exc(t_("Error in file format"));
	
	Vector<double> T; 	
    Vector<double> w;
    
	in.Seek(fpos);
		
	hd().head.Clear();
	while (!in.IsEof()) {
		f.Load(in.GetLine());
		double freq = f.GetDouble(0);
		double head = f.GetDouble(1);
		FindAdd(w, freq);
		FindAdd(hd().head, head);
	}
	
	if (hd().head.GetCount() == 0)
		throw Exc(Format(t_("Wrong format in Wamit file '%s'"), hd().file));
	
	if (!IsNull(hd().Nh) && hd().Nh != hd().head.GetCount())
		throw Exc(Format(t_("Number of headings is different than previous (%d != %d)"), hd().Nh, hd().head.GetCount()));
	hd().Nh = hd().head.GetCount();
	
	int Nf = w.GetCount();
	if (!IsNull(hd().Nf) && hd().Nf != Nf)
		throw Exc(Format(t_("Number of frequencies loaded is different than previous (%d != %d)"), hd().Nf, Nf));
	hd().Nf = Nf;
			
	if (w[0] > w[1]) {
		hd().dataFromW = false;
		T = pick(w);
		w.SetCount(hd().Nf);	
	} else {
		hd().dataFromW = true;
		T.SetCount(hd().Nf);
	}
	
	hd().Initialize_Forces(hd().ex);
	
	for (int ifr = 0; ifr < hd().Nf; ++ifr) {
		if (hd().dataFromW)
			T[ifr] = fround(2*M_PI/w[ifr], 8);
		else
			w[ifr] = fround(2*M_PI/T[ifr], 8);
	}

	if (hd().w.IsEmpty()) {
		hd().w = pick(w);
		hd().T = pick(T);
	} else if (!Compare(hd().w, w, 0.001))
		throw Exc(Format(t_("Frequencies loaded are different than previous\nPrevious: %s\nSeries:   %s"), ToString(hd().w), ToString(w)));
	else if (!Compare(hd().T, T, 0.001))
		throw Exc(Format(t_("Periods loaded are different than previous\nPrevious: %s\nSeries:   %s"), ToString(hd().T), ToString(T)));
	
	in.Seek(fpos);
	
	while (!in.IsEof()) {
		f.Load(in.GetLine());
		double freq = f.GetDouble(0);
		int ifr;
		if (hd().dataFromW)
		 	ifr = FindIndexRatio(hd().w, freq, 0.001);
		else
			ifr = FindIndexRatio(hd().T, freq, 0.001);
		if (ifr < 0)
			throw Exc(Format(t_("Unknown frequency %f"), freq));
		double head = f.GetDouble(1);
		int ih = FindIndexRatio(hd().head, head, 0.001);
		if (ih < 0)
			throw Exc(Format(t_("Unknown heading %f"), head));
			
		int i = f.GetInt(2) - 1;		
		
       	hd().ex.ma[ih](ifr, i) = f.GetDouble(3);
     	hd().ex.ph[ih](ifr, i) = f.GetDouble(4);
        hd().ex.re[ih](ifr, i) = f.GetDouble(5);
        hd().ex.im[ih](ifr, i) = f.GetDouble(6);
	}
		
	return true;
}

bool Wamit::Load_hst(String fileName) {
	hd().dimen = false;
	hd().len = 1;
	
	FileInLine in(fileName);
	if (!in.IsOpen())
		return false;
	FieldSplit f(in);
 
 	FileInLine::Pos fpos;
 	while (IsNull(ScanDouble(in.GetLine())) && !in.IsEof())
 		fpos = in.GetPos();
	if (in.IsEof())
		throw Exc(t_("Error in file format"));
	
	in.Seek(fpos);
	
	hd().C.SetCount(hd().Nb);
	for(int ibody = 0; ibody < hd().Nb; ++ibody)
		hd().C[ibody].setConstant(6, 6, 0);

	while (!in.IsEof()) {
		f.Load(in.GetLine());	
		int i = f.GetInt(0) - 1;
		int ib_i = i/6;
		i = i - ib_i*6;
		int j = f.GetInt(1) - 1;
		int ib_j = j/6;
		j = j - ib_j*6;
		if (ib_i == ib_j) 
			hd().C[ib_i](i, j) = f.GetDouble(2);
	}
		
	return true;
}

bool Wamit::Load_4(String fileName) {
	hd().dimen = false;
	hd().len = 1;
	
	FileInLine in(fileName);
	if (!in.IsOpen())
		return false;
	FieldSplit f(in);
 
 	FileInLine::Pos fpos;
 	while (IsNull(ScanDouble(in.GetLine())) && !in.IsEof())
 		fpos = in.GetPos();
	if (in.IsEof())
		throw Exc("Error in file format");
	
	Vector<double> T; 	
    Vector<double> w;
    
	in.Seek(fpos);
	
	int maxDof = 0;
	hd().head.Clear();
	while (!in.IsEof()) {
		f.Load(in.GetLine());
		double freq = f.GetDouble(0);
		FindAdd(w, freq);
		
		int dof = f.GetInt(2);
		if (dof > maxDof)
			maxDof = dof-1;
		
		double head = f.GetDouble(1);
		
		FindAdd(hd().head, head);
	}

	if (hd().head.GetCount() == 0)
		throw Exc(Format(t_("Wrong format in Wamit file '%s'"), hd().file));
	
	if (!IsNull(hd().Nh) && hd().Nh != hd().head.GetCount())
		throw Exc(Format(t_("Number of headings loaded is different than previous (%d != %d)"), hd().Nh, hd().head.GetCount()));
	hd().Nh = hd().head.GetCount();
	
	int Nb = 1 + int(maxDof/6);
	if (!IsNull(hd().Nb) && hd().Nb < Nb)
		throw Exc(Format(t_("Number of bodies loaded is lower than previous (%d != %d)"), hd().Nb, Nb));
	hd().Nb = Nb;
	
	int Nf = w.GetCount();
	if (!IsNull(hd().Nf) && hd().Nf != Nf)
		throw Exc(Format(t_("Number of frequencies loaded is different than previous (%d != %d)"), hd().Nf, Nf));
	hd().Nf = Nf;
	
	if (hd().Nb == 0 || hd().Nf < 2)
		throw Exc(Format(t_("Wrong format in Wamit file '%s'"), hd().file));
	
	if (w[0] > w[1]) {
		hd().dataFromW = false;
		T = pick(w);
		w.SetCount(hd().Nf);	
	} else {
		hd().dataFromW = true;
		T.SetCount(hd().Nf);
	}
	
	hd().Initialize_RAO();
	
	for (int ifr = 0; ifr < hd().Nf; ++ifr) {
		if (hd().dataFromW)
			T[ifr] = fround(2*M_PI/w[ifr], 8);
		else
			w[ifr] = fround(2*M_PI/T[ifr], 8);
	}
	
	if (hd().w.IsEmpty()) {
		hd().w = pick(w);
		hd().T = pick(T);
	} else if (!Compare(hd().w, w, 0.001))
		throw Exc(Format(t_("Frequencies loaded are different than previous\nPrevious: %s\nSeries:   %s"), ToString(hd().w), ToString(w)));
	else if (!Compare(hd().T, T, 0.001))
		throw Exc(Format(t_("Periods loaded are different than previous\nPrevious: %s\nSeries:   %s"), ToString(hd().T), ToString(T)));
	
	in.Seek(fpos);
	
	while (!in.IsEof()) {
		f.Load(in.GetLine());
		double freq = f.GetDouble(0);
		int ifr;
		if (hd().dataFromW)
		 	ifr = FindIndexRatio(hd().w, freq, 0.001);
		else
			ifr = FindIndexRatio(hd().T, freq, 0.001);
		if (ifr < 0)
			throw Exc(Format(t_("Unknown frequency %f"), freq));
		double head = f.GetDouble(1);
		int ih = FindIndexRatio(hd().head, head, 0.001);
		if (ih < 0)
			throw Exc(Format(t_("Unknown heading %f"), head));
			
		int i = f.GetInt(2) - 1;		
		
       	hd().rao.ma[ih](ifr, i) = f.GetDouble(3);
     	hd().rao.ph[ih](ifr, i) = f.GetDouble(4);
        hd().rao.re[ih](ifr, i) = f.GetDouble(5);
        hd().rao.im[ih](ifr, i) = f.GetDouble(6);
	}
		
	return true;
}

void Wamit::Save_1(String fileName) {
	FileOut out(fileName);
	if (!out.IsOpen())
		throw Exc(Format(t_("Impossible to open '%s'"), fileName));
	
	if (hd().IsLoadedAw0()) {
		for (int i = 0; i < hd().Nb*6; ++i)  
			for (int j = 0; j < hd().Nb*6; ++j)
				if (!IsNull(hd().Aw0(i, j))) 
					out << Format(" %s %5d %5d %s\n", FormatWam(-1), i+1, j+1,
													  FormatWam(hd().Aw0(i, j)/(hd().rho*pow(hd().len, Hydro::GetK_AB(i, j)))));
	}
	if (hd().IsLoadedAwinf()) {
		for (int i = 0; i < hd().Nb*6; ++i)  
			for (int j = 0; j < hd().Nb*6; ++j)
				if (!IsNull(hd().Awinf(i, j))) 
					out << Format(" %s %5d %5d %s\n", FormatWam(0), i+1, j+1,
													  FormatWam(hd().Awinf(i, j)/(hd().rho*pow(hd().len, Hydro::GetK_AB(i, j)))));
	}
	
	if (hd().Nf < 2)
		throw Exc(t_("No enough data to save (at least 2 frequencies)"));
		
	Vector<double> *pdata;
	int ifr0, ifrEnd, ifrDelta;
	if (hd().dataFromW) 
		pdata = &hd().w;
	else
		pdata = &hd().T;
	Vector<double> &data = *pdata;
	
	if (((data[1] > data[0]) && hd().dataFromW) || ((data[1] < data[0]) && !hd().dataFromW)) {
		ifr0 = 0;
		ifrEnd = hd().Nf;
		ifrDelta = 1;
	} else {
		ifr0 = hd().Nf - 1;
		ifrEnd = -1;
		ifrDelta = -1;
	}
	
	if (hd().IsLoadedA() && hd().IsLoadedB()) {
		for (int ifr = ifr0; ifr != ifrEnd; ifr += ifrDelta)
			for (int i = 0; i < hd().Nb*6; ++i)  
				for (int j = 0; j < hd().Nb*6; ++j)
					if (!IsNull(hd().A[ifr](i, j)) && !IsNull(hd().B[ifr](i, j))) 
						out << Format(" %s %5d %5d %s %s\n", FormatWam(data[ifr]), i+1, j+1,
										FormatWam(hd().A[ifr](i, j)/(hd().rho*pow(hd().len, Hydro::GetK_AB(i, j)))), 
										FormatWam(hd().B[ifr](i, j)/(hd().rho*pow(hd().len, Hydro::GetK_AB(i, j))*hd().w[ifr])));
	}
}

void Wamit::Save_3(String fileName) {
	FileOut out(fileName);
	if (!out.IsOpen())
		throw Exc(Format(t_("Impossible to open '%s'"), fileName));

	if (hd().Nf < 2)
		throw Exc(t_("No enough data to save (at least 2 frequencies)"));

	Vector<double> *pdata;
	int ifr0, ifrEnd, ifrDelta;
	if (hd().dataFromW) 
		pdata = &hd().w;
	else
		pdata = &hd().T;
	Vector<double> &data = *pdata;
	
	if (((data[1] > data[0]) && hd().dataFromW) || ((data[1] < data[0]) && !hd().dataFromW)) {
		ifr0 = 0;
		ifrEnd = hd().Nf;
		ifrDelta = 1;
	} else {
		ifr0 = hd().Nf - 1;
		ifrEnd = -1;
		ifrDelta = -1;
	}
	
	if (hd().IsLoadedFex()) {
		for (int ifr = ifr0; ifr != ifrEnd; ifr += ifrDelta)
			for (int ih = 0; ih < hd().Nh; ++ih)
				for (int i = 0; i < hd().Nb*6; ++i)
					if (!IsNull(hd().ex.ma[ih](ifr, i))) {
						double k = hd().g*hd().rho*pow(hd().len, Hydro::GetK_F(i));
						out << Format(" %s %s %5d %s %s %s %s\n", FormatWam(data[ifr]), FormatWam(hd().head[ih]), i+1,
										FormatWam(hd().ex.ma[ih](ifr, i)/k), FormatWam(hd().ex.ph[ih](ifr, i)),
										FormatWam(hd().ex.re[ih](ifr, i)/k), FormatWam(hd().ex.im[ih](ifr, i)/k));
					}
	}
}

void Wamit::Save_hst(String fileName) {
	FileOut out(fileName);
	if (!out.IsOpen())
		throw Exc(Format(t_("Impossible to open '%s'"), fileName));

	if (hd().IsLoadedC()) {
		for (int i = 0; i < 6*hd().Nb; ++i)  
			for (int j = 0; j < 6*hd().Nb; ++j) {
				int ib_i = i/6;
				int ii = i - ib_i*6;
				int ib_j = j/6;
				int jj = j - ib_j*6;
				if (!IsNull(hd().C[ib_i](ii, jj))) 
					out << Format(" %5d %5d  %s\n", i+1, j+1, FormatWam(hd().C[ib_i](ii, jj)/(hd().g*hd().rho*pow(hd().len, Hydro::GetK_C(i, j)))));
			}
	}
}

void Wamit::Save_4(String fileName) {
	FileOut out(fileName);
	if (!out.IsOpen())
		throw Exc(Format(t_("Impossible to open '%s'"), fileName));
	
	if (hd().Nf < 2)
		throw Exc(t_("No enough data to save (at least 2 frequencies)"));
		
	Vector<double> *pdata;
	int ifr0, ifrEnd, ifrDelta;
	if (hd().dataFromW) 
		pdata = &hd().w;
	else
		pdata = &hd().T;
	Vector<double> &data = *pdata;
	
	if (((data[1] > data[0]) && hd().dataFromW) || ((data[1] < data[0]) && !hd().dataFromW)) {
		ifr0 = 0;
		ifrEnd = hd().Nf;
		ifrDelta = 1;
	} else {
		ifr0 = hd().Nf - 1;
		ifrEnd = -1;
		ifrDelta = -1;
	}
	
	double A = 1;
	if (hd().IsLoadedRAO()) {
		for (int ifr = ifr0; ifr != ifrEnd; ifr += ifrDelta)
			for (int ih = 0; ih < hd().Nh; ++ih)
				for (int i = 0; i < hd().Nb*6; ++i)
					if (!IsNull(hd().rao.ma[ih](ifr, i))) {
						double k = A/pow(hd().len, Hydro::GetK_RAO(i));
						out << Format(" %s %s %5d %s %s %s %s\n", FormatWam(data[ifr]), FormatWam(hd().head[ih]), i+1,
										FormatWam(hd().rao.ma[ih](ifr, i)/k), FormatWam(hd().rao.ph[ih](ifr, i)),
										FormatWam(hd().rao.re[ih](ifr, i)/k), FormatWam(hd().rao.im[ih](ifr, i)/k));
					}
	}
}
					

bool Wamit::LoadDatMesh(String fileName) {
	FileInLine in(fileName);
	if (!in.IsOpen()) {
		hd().PrintError("\n" + Format(t_("Impossible to open file '%s'"), fileName));
		mh().lastError = Format(t_("Impossible to open file '%s'"), fileName);
		return false;
	}
	mh().file = fileName;
	mh.SetCode(MeshData::WAMIT_DAT);
	
	String line;
	FieldSplit f(in);	
	
	try {
		line = ToUpper(TrimBoth(in.GetLine()));
		if (!line.StartsWith("ZONE"))
			throw Exc(t_("'ZONE' field not found"));
		line.Replace("\"", "");
		line.Replace(" ", "");
		
		int pos;
		int T = Null;
		pos = line.FindAfter("T=");
		if (pos > 0) 
			T = ScanInt(line.Mid(pos));
		int I = Null;
		pos = line.FindAfter("I=");
		if (pos > 0) 
			I = ScanInt(line.Mid(pos));
		int J = Null;
		pos = line.FindAfter("J=");
		if (pos > 0) 
			J = ScanInt(line.Mid(pos));
		String F;
		pos = line.FindAfter("F=");
		if (pos > 0) 
			F = line.Mid(pos);
		
		if (IsNull(T)) {
			while(!in.IsEof()) {
				int id0 = mh().nodes.GetCount();
				for (int i = 0; i < I*J; ++i) {
					line = in.GetLine();	
					f.Load(line);
					
					double x = f.GetDouble(0);	
					double y = f.GetDouble(1);	
					double z = f.GetDouble(2);	
						
					Point3D &node = mh().nodes.Add();
					node.x = x;
					node.y = y;
					node.z = z;
				}
				for (int i = 0; i < I-1; ++i) {
					for (int j = 0; j < J-1; ++j) {
						Panel &panel = mh().panels.Add();
						panel.id[0] = id0 + I*j     + i;
						panel.id[1] = id0 + I*j     + i+1;
						panel.id[2] = id0 + I*(j+1) + i;
						panel.id[3] = id0 + I*(j+1) + i+1;
					}
				}
				in.GetLine();
			}
		} else {
			for (int i = 0; i < I; ++i) {
				line = in.GetLine();	
				f.Load(line);
				
				double x = f.GetDouble(0);	
				double y = f.GetDouble(1);	
				double z = f.GetDouble(2);	
					
				Point3D &node = mh().nodes.Add();
				node.x = x;
				node.y = y;
				node.z = z;
			}
			for (int i = 0; i < I/4; ++i) {
				line = in.GetLine();	
				f.Load(line);
				
				Panel &panel = mh().panels.Add();
				for (int i = 0; i < 4; ++i)
					panel.id[i] = f.GetInt(i) - 1;
			}
		}
		mh().GetLimits();
	} catch (Exc e) {
		hd().PrintError(Format("\n%s: %s", t_("Error"), e));
		mh().lastError = e;
		return false;
	}
	
	return true;
}
	
bool Wamit::LoadGdfMesh(String fileName) {
	FileInLine in(fileName);
	if (!in.IsOpen()) {
		hd().PrintError("\n" + Format(t_("Impossible to open file '%s'"), fileName));
		mh().lastError = Format(t_("Impossible to open file '%s'"), fileName);
		return false;
	}
	mh().file = fileName;
	mh.SetCode(MeshData::WAMIT_GDF);
	
	String line;
	FieldSplit f(in);	
	
	try {
		in.GetLine();
		line = in.GetLine();	
		f.Load(line);
		double scale = f.GetDouble(0);
		if (scale < 1)
			throw Exc(t_("Wrong scale in .gdf file"));
					
		line = in.GetLine();	
		f.Load(line);
		mh().y0z = f.GetInt(0) != 0;
		mh().x0z = f.GetInt(1) != 0;
		
		line = in.GetLine();	
		f.Load(line);
		int nPatches = f.GetInt(0);
		if (nPatches < 1)
			throw Exc(t_("Wrong number of patches in .gdf file"));
				
		mh().nodes.Clear();
		mh().panels.Clear();
		
		while(!in.IsEof()) {
			int ids[4];
			for (int i = 0; i < 4; ++i) {
				line = in.GetLine();	
				f.Load(line);
				
				double x = f.GetDouble(0)*scale;	
				double y = f.GetDouble(1)*scale;	
				double z = f.GetDouble(2)*scale;	
				
				bool found = false;
				for (int in = 0; in < mh().nodes.GetCount(); ++in) {
					Point3D &node = mh().nodes[in];
					if (x == node.x && y == node.y && z == node.z) {
						ids[i] = in;
						found = true;
						break;
					}
				}
				if (!found) {
					Point3D &node = mh().nodes.Add();
					node.x = x;
					node.y = y;
					node.z = z;
					ids[i] = mh().nodes.GetCount() - 1;
				}
			}
			Panel &panel = mh().panels.Add();
			for (int i = 0; i < 4; ++i)
				panel.id[i] = ids[i];
		}
		if (mh().panels.GetCount() != nPatches)
			throw Exc(t_("Wrong number of patches in .gdf file"));
		//if (mh().Check())
		//	throw Exc(t_("Wrong nodes found in Wamit .gdf mesh file"));
		mh().GetLimits();
	} catch (Exc e) {
		hd().PrintError(Format("\n%s: %s", t_("Error"), e));
		mh().lastError = e;
		return false;
	}
	
	return true;
}

void Wamit::SaveGdfMesh(String fileName) {
	FileOut out(fileName);
	if (!out.IsOpen())
		throw Exc(Format(t_("Impossible to open '%s'"), fileName));	
	
	out << Format("BEMRosetta GDF mesh file export (%s)\n", Format("%", GetSysTime()));
	out << Format("1 %f 	ULEN GRAV\n", hd().g_dim());
	out << Format("%d  %d 	ISX  ISY\n", mh().y0z ? 1 : 0, mh().x0z ? 1 : 0);
	out << Format("%d\n", mh().panels.GetCount());
	for (int ip = 0; ip < mh().panels.GetCount(); ++ip) {
		for (int i = 0; i < 4; ++i) {
			int id = mh().panels[ip].id[i];
			Point3D &p = mh().nodes[id];
			out << "  " << p.x << " " << p.y << " " << p.z << "\n";
		}
	}
	 
}