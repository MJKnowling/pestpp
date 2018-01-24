#include <random>
#include <iomanip>
#include "Ensemble.h"
#include "RestartController.h"
#include "utilities.h"
#include "Ensemble.h"
#include "EnsembleSmoother.h"
#include "ObjectiveFunc.h"
#include "covariance.h"
#include "RedSVD-h.h"
#include "SVDPackage.h"


PhiHandler::PhiHandler(Pest *_pest_scenario, FileManager *_file_manager,
	ObservationEnsemble *_oe_base, ParameterEnsemble *_pe_base,
	Covariance *_parcov, double *_reg_factor)	
{
	pest_scenario = _pest_scenario;
	file_manager = _file_manager;
	oe_base = _oe_base;
	pe_base = _pe_base;
	reg_factor = _reg_factor;
	parcov_inv = _parcov->inv();
	//parcov.inv_ip();
	oreal_names = oe_base->get_real_names();
	preal_names = pe_base->get_real_names();
	prepare_csv(file_manager->open_ofile_ext("phi.actual.csv"),oreal_names);
	prepare_csv(file_manager->open_ofile_ext("phi.meas.csv"),oreal_names);
	prepare_csv(file_manager->open_ofile_ext("phi.composite.csv"),oreal_names);
	prepare_csv(file_manager->open_ofile_ext("phi.regul.csv"),preal_names);

}


void PhiHandler::update(ObservationEnsemble & oe, ParameterEnsemble & pe)
{
	//update the various phi component vectors
	meas.clear();
	meas = calc_meas(oe);
	regul.clear();
	regul = calc_regul(pe);
	actual.clear();
	actual = calc_actual(oe);
	composite.clear();
	composite = calc_composite(meas,regul);

}

map<string, double>* PhiHandler::get_phi_map(PhiHandler::phiType &pt)
{
	switch (pt)
	{
	case PhiHandler::phiType::ACTUAL:
		return &actual;
	case PhiHandler::phiType::COMPOSITE:
		return &composite;
	case PhiHandler::phiType::MEAS:
		return &meas;
	case PhiHandler::phiType::REGUL:
		return &regul;
	}
}


double PhiHandler::get_mean(phiType pt)
{
	double mean = 0.0;
	map<string, double>* phi_map = get_phi_map(pt);
	map<string, double>::iterator pi = phi_map->begin(), end = phi_map->end();
	for (pi;pi != end; ++pi)
		mean = mean + pi->second;
	return mean / phi_map->size();
}

double PhiHandler::get_std(phiType pt)
{
	double mean = get_mean(pt);
	double var = 0.0;
	map<string, double>* phi_map = get_phi_map(pt);
	map<string, double>::iterator pi = phi_map->begin(), end = phi_map->end();
	for (pi; pi != end; ++pi)
		var = var + (pow(pi->second - mean,2));
	return sqrt(var/(phi_map->size()));
}

double PhiHandler::get_max(phiType pt)
{
	double mx = -1.0e+30;
	map<string, double>* phi_map = get_phi_map(pt);
	map<string, double>::iterator pi = phi_map->begin(), end = phi_map->end();
	for (pi; pi != end; ++pi)
		mx = (pi->second > mx) ? pi->second : mx;
	return mx;
}

double PhiHandler::get_min(phiType pt)
{
	double mn = 1.0e+30;
	map<string, double>* phi_map = get_phi_map(pt);
	map<string, double>::iterator pi = phi_map->begin(), end = phi_map->end();
	for (pi; pi != end; ++pi)
		mn = (pi->second < mn) ? pi->second : mn;
	return mn;
}

map<string, double> PhiHandler::get_summary_stats(PhiHandler::phiType pt)
{
	map<string, double> stats;
	stats["mean"] = get_mean(pt);
	stats["std"] = get_std(pt);
	stats["max"] = get_max(pt);
	stats["min"] = get_min(pt);
	return stats;
}

string PhiHandler::get_summary_string(PhiHandler::phiType pt)
{
	map<string, double> stats = get_summary_stats(pt);
	stringstream ss;
	string typ;
	switch (pt)
	{
	case PhiHandler::phiType::ACTUAL:
		typ = "actual";
		break;
	case PhiHandler::phiType::MEAS:
		typ = "measured";
		break;
	case PhiHandler::phiType::REGUL:
		typ = "regularization";
		break;
	case PhiHandler::phiType::COMPOSITE:
		typ = "composite";
		break;
	}

	ss << setw(15) << typ << setw(15) << stats["mean"] << setw(15) << stats["std"] << setw(15) << stats["min"] << setw(15) << stats["max"] << endl;
	return ss.str();
}


string PhiHandler::get_summary_header()
{
	stringstream ss;
	ss << setw(15) << "phi type" << setw(15) << "mean" << setw(15) << "std" << setw(15) << "min" << setw(15) << "max" << endl;
	return ss.str();

}


void PhiHandler::report()
{
	ofstream &f = file_manager->rec_ofstream();
	f << get_summary_header();
	cout << get_summary_header();
	string s = get_summary_string(PhiHandler::phiType::COMPOSITE);
	f << s;
	cout << s;
	s = get_summary_string(PhiHandler::phiType::MEAS);
	f << s;
	cout << s;
	s = get_summary_string(PhiHandler::phiType::REGUL);
	f << s;
	cout << s;
	s = get_summary_string(PhiHandler::phiType::ACTUAL);
	f << s;
	cout << s;
	f << endl << endl;
	f.flush();

}

void PhiHandler::write(int iter_num, int total_runs)
{
	write_csv(iter_num, total_runs, file_manager->get_ofstream("phi.actual.csv"), phiType::ACTUAL,oreal_names);
	write_csv(iter_num, total_runs, file_manager->get_ofstream("phi.meas.csv"), phiType::MEAS, oreal_names);
	write_csv(iter_num, total_runs, file_manager->get_ofstream("phi.regul.csv"), phiType::REGUL, preal_names);
	write_csv(iter_num, total_runs, file_manager->get_ofstream("phi.composite.csv"), phiType::COMPOSITE, oreal_names);
}

void PhiHandler::write_csv(int iter_num, int total_runs, ofstream &csv, phiType pt, vector<string> &names)
{
	map<string, double>* phi_map = get_phi_map(pt);
	map<string, double>::iterator pmi = phi_map->end();
	csv << iter_num << ',' << total_runs;
	map<string, double> stats = get_summary_stats(pt);
	csv << ',' << stats["mean"] << ',' << stats["std"] << ',' << stats["min"] << ',' << stats["max"];
	for (auto &name : names)
	{
		csv << ',';
		if (phi_map->find(name) != pmi)
			csv << phi_map->at(name);
	}
	csv << endl;
	csv.flush();
}

void PhiHandler::prepare_csv(ofstream & csv,vector<string> &names)
{
	csv << "iteration,total_runs,mean,standard_deviation,min,max";
	for (auto &name : names)
		csv << ',' << name;
	csv << endl;
}

vector<int> PhiHandler::get_idxs_greater_than(double bad_phi, ObservationEnsemble &oe)
{
	map<string, double> _meas = calc_meas(oe);
	vector<int> idxs;
	vector<string> names = oe.get_real_names();
	for (int i=0;i<names.size();i++)
		if (_meas[names[i]] > bad_phi)
			idxs.push_back(i);
	return idxs;
}

map<string, double> PhiHandler::calc_meas(ObservationEnsemble & oe)
{
	map<string, double> phi_map;
	ObservationInfo oinfo = pest_scenario->get_ctl_observation_info();
	Eigen::VectorXd oe_base_vec, oe_vec, q, diff;
	vector<string> act_obs_names = pest_scenario->get_ctl_ordered_nz_obs_names();
	q.resize(act_obs_names.size());
	double w;
	for (int i = 0; i < act_obs_names.size(); i++)
		q(i) = oinfo.get_weight(act_obs_names[i]);
	vector<string> base_real_names = oe_base->get_real_names(), oe_real_names = oe.get_real_names();
	vector<string>::iterator start = base_real_names.begin(), end = base_real_names.end();
	double phi;
	string rname;

	Eigen::MatrixXd oe_reals = oe.get_eigen(vector<string>(), oe_base->get_var_names());
	for (int i = 0; i<oe.shape().first; i++)
	{
		rname = oe_real_names[i];
		if (find(start, end, rname) == end)
			continue;
		oe_base_vec = oe_base->get_real_vector(rname);
		oe_vec = oe_reals.row(i);
		diff = (oe_vec - oe_base_vec).cwiseProduct(q);
		phi = (diff.cwiseProduct(diff)).sum();
		phi_map[rname] = phi;
	}
	return phi_map;
}

map<string, double> PhiHandler::calc_regul(ParameterEnsemble & pe)
{	
	map<string, double> phi_map;
	vector<string> real_names = pe.get_real_names();
	pe_base->transform_ip(ParameterEnsemble::transStatus::NUM);
	pe.transform_ip(ParameterEnsemble::transStatus::NUM);
	Eigen::MatrixXd diff_mat = pe.get_eigen() - pe_base->get_eigen(real_names, vector<string>());
	Eigen::VectorXd parcov_inv_diag = parcov_inv.e_ptr()->diagonal();
	Eigen::VectorXd diff;
	for (int i = 0; i < real_names.size(); i++)
	{	
		diff = diff_mat.row(i);
		diff = diff.cwiseProduct(diff);
		diff = diff.cwiseProduct(parcov_inv_diag);
		phi_map[real_names[i]] = diff.sum();
	}
	return phi_map;
}

map<string, double> PhiHandler::calc_actual(ObservationEnsemble & oe)
{
	map<string, double> phi_map;
	ObservationInfo oinfo = pest_scenario->get_ctl_observation_info();
	Observations obs = pest_scenario->get_ctl_observations();
	Eigen::VectorXd obs_val_vec, oe_vec, q, diff;
	vector<string> act_obs_names = pest_scenario->get_ctl_ordered_nz_obs_names();
	q.resize(act_obs_names.size());
	obs_val_vec.resize(act_obs_names.size());
	double w;
	for (int i = 0; i < act_obs_names.size(); i++)
	{
		q(i) = oinfo.get_weight(act_obs_names[i]);
		obs_val_vec(i) = obs[act_obs_names[i]];
	}
		
	vector<string> base_real_names = oe_base->get_real_names(), oe_real_names = oe.get_real_names();
	vector<string>::iterator start = base_real_names.begin(), end = base_real_names.end();
	double phi;
	string rname;

	Eigen::MatrixXd oe_reals = oe.get_eigen(vector<string>(), oe_base->get_var_names());
	for (int i = 0; i<oe.shape().first; i++)
	{
		rname = oe_real_names[i];
		if (find(start, end, rname) == end)
			continue;
		oe_vec = oe_reals.row(i);
		diff = (oe_vec - obs_val_vec).cwiseProduct(q);
		phi = (diff.cwiseProduct(diff)).sum();
		phi_map[rname] = phi;
	}
	return phi_map;
}


map<string, double> PhiHandler::calc_composite(map<string, double> &_meas, map<string, double> &_regul)
{
	map<string, double> phi_map;
	string prn, orn;
	double reg, mea;
	map<string, double>::iterator meas_end = _meas.end(), regul_end = _regul.end();
	for (int i = 0; i < oe_base->shape().first; i++)
	{
		prn = preal_names[i];
		orn = oreal_names[i];
		if (meas.find(orn) != meas_end)
		{
			mea = _meas[orn];
			reg = _regul[prn];
			phi_map[orn] = mea + (reg * *reg_factor);
		}
	}
	return phi_map;
}	




IterEnsembleSmoother::IterEnsembleSmoother(Pest &_pest_scenario, FileManager &_file_manager,
	OutputFileWriter &_output_file_writer, PerformanceLog *_performance_log,
	RunManagerAbstract* _run_mgr_ptr) : pest_scenario(_pest_scenario), file_manager(_file_manager),
	output_file_writer(_output_file_writer), performance_log(_performance_log),
	run_mgr_ptr(_run_mgr_ptr)
{
	pe.set_pest_scenario(&pest_scenario);
	oe.set_pest_scenario(&pest_scenario);
}

void IterEnsembleSmoother::throw_ies_error(string &message)
{
	performance_log->log_event("IterEnsembleSmoother error: " + message);
	cout << endl << "   ************   " << endl << "    IterEnsembleSmoother error: " << message << endl << endl;
	file_manager.rec_ofstream() << endl << "   ************   " << endl << "    IterEnsembleSmoother error: " << message << endl << endl;
	file_manager.close_file("rec");
	performance_log->~PerformanceLog();
	throw runtime_error("IterEnsembleSmoother error: " + message);
}

void IterEnsembleSmoother::initialize_pe(Covariance &cov)
{
	stringstream ss;
	int num_reals = pest_scenario.get_pestpp_options().get_ies_num_reals();
	string par_csv = pest_scenario.get_pestpp_options().get_ies_par_csv();
	if (par_csv.size() == 0)
	{
		message(1, "drawing parameter realizations: ", num_reals);
		pe.draw(num_reals, cov);
		stringstream ss;
		ss << file_manager.get_base_filename() << ".0.par.csv";
		message(1, "saving initial parameter ensemble to ", ss.str());
		pe.to_csv(ss.str());
	}
	else
	{
		string par_ext = pest_utils::lower_cp(par_csv).substr(par_csv.size() - 3, par_csv.size());
		performance_log->log_event("processing par csv " + par_csv);
		if (par_ext.compare("csv") == 0)
		{
			message(1, "loading par ensemble from csv file", par_csv);
			try
			{
				pe.from_csv(par_csv);
			}
			catch (const exception &e)
			{
				ss << "error processing par csv: " << e.what();
				throw_ies_error(ss.str());
			}
			catch (...)
			{
				throw_ies_error(string("error processing par csv"));
			}
		}
		else if ((par_ext.compare("jcb") == 0) || (par_ext.compare("jco") == 0))
		{
			message(1, "loading par ensemble from binary file", par_csv);
			try
			{
				pe.from_binary(par_csv);
			}
			catch (const exception &e)
			{
				ss << "error processing par jcb: " << e.what();
				throw_ies_error(ss.str());
			}
			catch (...)
			{
				throw_ies_error(string("error processing par jcb"));
			}
		}
		else
		{
			ss << "unrecognized par csv extension " << par_ext << ", looking for csv, jcb, or jco";
			throw_ies_error(ss.str());
		}
		message(1, "initializing prior parameter covariance matrix from ensemble (using diagonal matrix)");
		parcov = pe.get_diagonal_cov_matrix();
		
	}
	
}

void IterEnsembleSmoother::add_bases()
{
	//check that 'base' isn't already in ensemble
	vector<string> rnames = pe.get_real_names();
	if (find(rnames.begin(), rnames.end(), "base") != rnames.end())
	{
		message(1, "'base' realization already in parameter ensemble, ignoring '++ies_include_base'");
	}
	else
	{
		message(1, "adding 'base' parameter values to ensemble");
		Parameters pars = pest_scenario.get_ctl_parameters();
		pe.get_par_transform().active_ctl2numeric_ip(pars);
		pe.append("base", pars);
	}
	
	//check that 'base' isn't already in ensemble
	rnames = oe.get_real_names();
	if (find(rnames.begin(), rnames.end(), "base") != rnames.end())
	{		
		message(1, "'base' realization already in observation ensemble, ignoring '++ies_include_base'");
	}
	else
	{
			
		message(1, "adding 'base' observation values to ensemble");
		Observations obs = pest_scenario.get_ctl_observations();
		oe.append("base", obs);
	}
}

void IterEnsembleSmoother::initialize_oe(Covariance &cov)
{
	stringstream ss;
	int num_reals = pe.shape().first;
	performance_log->log_event("load obscov");
	string obs_csv = pest_scenario.get_pestpp_options().get_ies_obs_csv();
	if (obs_csv.size() == 0)
	{
		message(1, "drawing observation noise realizations: ", num_reals);
		oe.draw(num_reals, cov);
		stringstream ss;
		ss << file_manager.get_base_filename() << ".0.obs.csv";
		message(1, "saving initial observation ensemble to ", ss.str());
		oe.to_csv(ss.str());
	}
	else
	{
		string obs_ext = pest_utils::lower_cp(obs_csv).substr(obs_csv.size() - 3, obs_csv.size());
		performance_log->log_event("processing obs csv " + obs_csv);
		if (obs_ext.compare("csv") == 0)
		{
			message(1, "loading obs ensemble from csv file", obs_csv);
			try
			{
				oe.from_csv(obs_csv);
			}
			catch (const exception &e)
			{
				ss << "error processing obs csv: " << e.what();
				throw_ies_error(ss.str());
			}
			catch (...)
			{
				throw_ies_error(string("error processing obs csv"));
			}
		}
		else if ((obs_ext.compare("jcb") == 0) || (obs_ext.compare("jco") == 0))
		{
			message(1, "loading obs ensemble from binary file", obs_csv);
			try
			{
				oe.from_binary(obs_csv);
			}
			catch (const exception &e)
			{
				stringstream ss;
				ss << "error processing obs binary file: " << e.what();
				throw_ies_error(ss.str());
			}
			catch (...)
			{
				throw_ies_error(string("error processing obs binary file"));
			}
		}
		else
		{
			ss << "unrecognized obs ensemble extension " << obs_ext << ", looing for csv, jcb, or jco";
			throw_ies_error(ss.str());
		}
	}
	
}

template<typename T, typename A>
void IterEnsembleSmoother::message(int level, string &_message, vector<T, A> _extras)
{
	stringstream ss;
	if (level == 0)
		ss << endl << "  ---  ";
	else if (level == 1)
		ss << "...";
	ss << _message;
	if (_extras.size() > 0)
	{
		
		for (auto &e : _extras)
			ss << e << " , ";
		
	}
	if (level == 0)
		ss << "  ---  ";

	cout << ss.str() << endl;
	file_manager.rec_ofstream() <<ss.str() << endl;

}

void IterEnsembleSmoother::message(int level, string &_message)
{
	message(level, _message, vector<string>());
}

template<typename T>
void IterEnsembleSmoother::message(int level, string &_message, T extra)
{
	stringstream ss;
	ss << _message << " " << extra;
	message(level, ss.str());
}

void IterEnsembleSmoother::sanity_checks()
{
	PestppOptions* ppo = pest_scenario.get_pestpp_options_ptr();
	string par_csv = ppo->get_ies_par_csv();
	string obs_csv = ppo->get_ies_obs_csv();
	string restart = ppo->get_ies_obs_restart_csv();
	vector<string> errors;
	vector<string> warnings;
	stringstream ss;
	if ((par_csv.size() == 0) && (restart.size() > 0))
		errors.push_back("ies_par_csv is empty but ies_restart_obs_csv is not - how can this work?");
	if (ppo->get_ies_bad_phi() <= 0.0)
		errors.push_back("ies_bad_phi <= 0.0, really?");
	if ((ppo->get_ies_num_reals() < error_min_reals) && (par_csv.size() == 0))
	{
		ss.str("");
		ss << "ies_num_reals < " << error_min_reals << ", this is redic, increaing to " << warn_min_reals;
		warnings.push_back(ss.str());
		ppo->set_ies_num_reals(warn_min_reals);
	}
	if ((ppo->get_ies_num_reals() < warn_min_reals) && (par_csv.size() == 0))
	{
		ss.str("");
		ss << "ies_num_reals < " << warn_min_reals << ", this is prob too few";
		warnings.push_back(ss.str());
	}
	if (ppo->get_ies_reg_factor() < 0.0)
		errors.push_back("ies_reg_factor < 0.0 - WRONG!");
	if (ppo->get_ies_reg_factor() > 1.0)
		errors.push_back("ies_reg_factor > 1.0 - nope");
	if ((par_csv.size() == 0) && (ppo->get_ies_subset_size() < 10000000) && (ppo->get_ies_num_reals() < ppo->get_ies_subset_size() * 2))
		warnings.push_back("ies_num_reals < 2*ies_subset_size: you not gaining that much using subset here");
	if ((ppo->get_ies_subset_size() < 100000001) && (ppo->get_ies_lam_mults().size() == 1))
	{
		warnings.push_back("only one lambda mult to test, no point in using a subset");
		//ppo->set_ies_subset_size(100000000);
	}
	if ((ppo->get_ies_verbose_level() < 0) || (ppo->get_ies_verbose_level() > 2))
	{
		warnings.push_back("ies_verbose_level must be between 0 and 2, resetting to 2");
		ppo->set_ies_verbose_level(2);
	}
	
	if (warnings.size() > 0)
	{
		message(0, "sanity_check warnings");
		for (auto &w : warnings)
			message(1, w);
	}
	if (errors.size() > 0)
	{
		message(0, "sanity_check errors - uh oh");
		for (auto &e : errors)
			message(1, e);
		throw_ies_error(string("sanity_check() found some problems - please review rec file"));
	}
	cout << endl << endl;
}

void IterEnsembleSmoother::initialize()
{
	message(0, "initializing");

	

	verbose_level = pest_scenario.get_pestpp_options_ptr()->get_ies_verbose_level();
	iter = 0;
	//ofstream &frec = file_manager.rec_ofstream();
	last_best_mean = 1.0E+30;
	last_best_std = 1.0e+30;
	lambda_max = 1.0E+10;
	lambda_min = 1.0E-10;
	warn_min_reals = 30;
	error_min_reals = 3;
	lam_mults = pest_scenario.get_pestpp_options().get_ies_lam_mults();
	if (lam_mults.size() == 0)
		lam_mults.push_back(1.0);
	message(1, "using lambda multipliers: ", lam_mults);
	
	sanity_checks();
	
	bool echo = false;
	if (verbose_level > 1)
		echo = true;

	act_obs_names = pest_scenario.get_ctl_ordered_nz_obs_names();
	act_par_names = pest_scenario.get_ctl_ordered_adj_par_names();

	subset_size = pest_scenario.get_pestpp_options().get_ies_subset_size();
	reg_factor = pest_scenario.get_pestpp_options().get_ies_reg_factor();
	message(1,"using reg_factor: ", reg_factor);
	double bad_phi = pest_scenario.get_pestpp_options().get_ies_bad_phi();
	if (bad_phi < 1.0e+30)
		message(1, "using bad_phi: ", bad_phi);

	int num_reals = pest_scenario.get_pestpp_options().get_ies_num_reals();
	
	stringstream ss;
	performance_log->log_event("load parcov");
	string parcov_filename = pest_scenario.get_pestpp_options().get_parcov_filename();
	
	if (parcov_filename.size() == 0)
	{
		//if a par ensemvble arg wasn't passed, use par bounds, otherwise, construct diagonal parcov from par ensemble later
		if (pest_scenario.get_pestpp_options().get_ies_par_csv().size() == 0)
		{
			message(0, "initializing prior parameter covariance matrix from parameter bounds");
			parcov.from_parameter_bounds(pest_scenario);
		}
	}
	else
	{
		message(0, "initializing prior parameter covariance matrix from file", parcov_filename);
		string extension = parcov_filename.substr(parcov_filename.size() - 3, 3);
		pest_utils::upper_ip(extension);
		if (extension.compare("COV") == 0)
			parcov.from_ascii(parcov_filename);
		else if ((extension.compare("JCO") == 0) || (extension.compare("JCB") == 0))
			parcov.from_binary(parcov_filename);
		else if (extension.compare("UNC") == 0)
			parcov.from_uncertainty_file(parcov_filename);
		else
			throw_ies_error("unrecognized parcov_filename extension: " + extension);
	}	
	if (parcov.e_ptr()->rows() > 0)
		parcov = parcov.get(act_par_names);
	
	initialize_pe(parcov); //not inverted yet..
	
	//jwhite 23jan2018: not inverting this anymore - pushes parameter values around too much.  Works better without inverting
	/*performance_log->log_event("inverting parcov");
	message(1, "inverting prior parameter covariance matrix");
	parcov.inv_ip(echo);*/


	//obs ensemble
	message(1, "initializing observation noise covariance matrix from observation weights");
	Covariance obscov;
	obscov.from_observation_weights(pest_scenario);
	obscov = obscov.get(act_obs_names);
	initialize_oe(obscov);

	if (pe.shape().first != oe.shape().first)
	{
		ss.str("");
		ss << "parameter ensemble rows (" << pe.shape().first << ") not equal to observation ensemble rows (" << oe.shape().first << ")";
		throw_ies_error(ss.str());
	}

	if (subset_size > pe.shape().first)
	{
		use_subset = false;
	}
	else
	{
		message(0, "using subset in lambda testing, only first ", subset_size);
		use_subset = true;
	}
	
	

	//need this here for Am calcs...
	message(0, "transforming parameter ensembles to numeric");
	pe.transform_ip(ParameterEnsemble::transStatus::NUM);
	
	if (pest_scenario.get_pestpp_options().get_ies_include_base())
	{
		add_bases();
	}
	
	oe_org_real_names = oe.get_real_names();
	pe_org_real_names = pe.get_real_names();

	oe_base = oe; //copy
	//reorder this for later...
	oe_base.reorder(vector<string>(), act_obs_names);

	pe_base = pe; //copy
	//reorder this for later
	pe_base.reorder(vector<string>(), act_par_names);
	
	obscov.inv_ip(echo);
	obscov_inv_sqrt = obscov.get_matrix().diagonal().cwiseSqrt().asDiagonal();
	
	if (!pest_scenario.get_pestpp_options().get_ies_use_approx()) 
	{
		message(0, "using full (MAP) update solution");
		performance_log->log_event("calculating 'Am' matrix for full solution");
		message(1, "forming Am matrix");
		double scale = (1.0 / (sqrt(double(pe.shape().first - 1))));
		Eigen::MatrixXd par_diff = scale * pe.get_eigen_mean_diff();
		par_diff.transposeInPlace();
		if (verbose_level > 1)
		{
			cout << "prior_par_diff: " << par_diff.rows() << ',' << par_diff.cols() << endl;
			if (verbose_level > 2)
				save_mat("prior_par_diff.dat", par_diff);
		}
		
		Eigen::MatrixXd ivec, upgrade_1, s, V, U, st;
		SVD_REDSVD rsvd;
		rsvd.set_performance_log(performance_log);

		rsvd.solve_ip(par_diff, s, U, V, pest_scenario.get_svd_info().eigthresh, pest_scenario.get_svd_info().maxsing);
		par_diff.resize(0, 0);
		Eigen::MatrixXd temp = s.asDiagonal();
		Eigen::MatrixXd temp2 = temp.inverse();
		Am = U * temp;
		if (verbose_level > 1)
		{
			cout << "Am:" << Am.rows() << ',' << Am.cols() << endl;
			if (verbose_level > 2)
			{
				save_mat("am.dat", Am);
				save_mat("am_u.dat", U);
				save_mat("am_v.dat", V);
				save_mat("am_s_inv.dat", temp2);
			}
		}	
	}


	string obs_restart_csv = pest_scenario.get_pestpp_options().get_ies_obs_restart_csv();	
	//no restart
	if (obs_restart_csv.size() == 0)
	{
		performance_log->log_event("running initial ensemble");
		message(0, "running initial ensemble of size", oe.shape().first);
		run_ensemble(pe, oe);
		pe.transform_ip(ParameterEnsemble::transStatus::NUM);
	}
	else
	{
		performance_log->log_event("restart IES with existing obs ensemble csv: " + obs_restart_csv);
		message(0, "restarting with existing obs ensemble", obs_restart_csv);
		string obs_ext = pest_utils::lower_cp(obs_restart_csv).substr(obs_restart_csv.size() - 3, obs_restart_csv.size());
		if (obs_ext.compare("csv") == 0)
		{
			message(1, "loading restart obs ensemble from csv file", obs_restart_csv);
			try
			{
				oe.from_csv(obs_restart_csv);
			}
			catch (const exception &e)
			{
				ss << "error processing restart obs csv: " << e.what();
				throw_ies_error(ss.str());
			}
			catch (...)
			{
				throw_ies_error(string("error processing restart obs csv"));
			}
		}
		else if ((obs_ext.compare("jcb") == 0) || (obs_ext.compare("jco") == 0))
		{
			message(1, "loading restart obs ensemble from binary file", obs_restart_csv);
			try
			{
				oe.from_binary(obs_restart_csv);
			}
			catch (const exception &e)
			{
				ss << "error processing restart obs binary file: " << e.what();
				throw_ies_error(ss.str());
			}
			catch (...)
			{
				throw_ies_error(string("error processing restart obs binary file"));
			}
		}
		else
		{
			ss << "unrecognized restart obs ensemble extension " << obs_ext << ", looing for csv, jcb, or jco";
			throw_ies_error(ss.str());
		}
		//check that restart oe is in sync
		stringstream ss;
		vector<string> oe_real_names = oe.get_real_names(),oe_base_real_names = oe_base.get_real_names();
		vector<string>::const_iterator start, end;
		vector<string> missing;
		start = oe_base_real_names.begin();
		end = oe_base_real_names.end();
		for (auto &rname : oe_real_names)
			if (find(start, end, rname) == end)
				missing.push_back(rname);
		if (missing.size() > 0)
		{
			ss << "the following realization names were not found in the restart obs csv:";
			for (auto &m : missing)
				ss << m << ",";
			throw_ies_error(ss.str());

		}


		if (oe.shape().first < oe_base.shape().first) //maybe some runs failed...
		{		
			//find which realizations are missing and reorder oe_base, pe and pe_base
			message(0, "shape mismatch detected with restart obs ensemble...checking for compatibility");
			vector<string> pe_real_names;
			start = oe_base_real_names.begin();
			end = oe_base_real_names.end();	
			vector<string>::const_iterator it;
			int iit;
			for (int i = 0; i < oe.shape().first; i++)
			{
				it = find(start, end, oe_real_names[i]);
				if (it != end)
				{
					iit = it - start;
					pe_real_names.push_back(pe_org_real_names[iit]);
				}
			}
			try
			{
				oe_base.reorder(oe_real_names, vector<string>());
			}
			catch (exception &e)
			{
				ss << "error reordering oe_base with restart oe:" << e.what();
				throw_ies_error(ss.str());
			}
			catch (...)
			{
				throw_ies_error(string("error reordering oe_base with restart oe"));
			}
			try
			{
				pe.reorder(pe_real_names, vector<string>());
			}
			catch (exception &e)
			{
				ss << "error reordering pe with restart oe:" << e.what();
				throw_ies_error(ss.str());
			}
			catch (...)
			{
				throw_ies_error(string("error reordering pe with restart oe"));
			}
		}
		else if (oe.shape().first > oe_base.shape().first) //something is wrong
		{
			ss << "restart oe has too many rows: " << oe.shape().first << " compared to oe_base: " << oe_base.shape().first;
			throw_ies_error(ss.str());
		}
	}


	performance_log->log_event("calc initial phi");
	ph = PhiHandler(&pest_scenario, &file_manager, &oe_base, &pe_base, &parcov, &reg_factor);
	drop_bad_phi(pe, oe);
	if (oe.shape().first == 0)
	{
		throw_ies_error(string("all realizations dropped as 'bad'"));
	}
	if (oe.shape().first <= error_min_reals)
	{
		message(0, "too few active realizations:", oe.shape().first);
		message(1, "need at least ", error_min_reals);
		throw_ies_error(string("too few active realizations, cannot continue"));
	}
	if (oe.shape().first <= warn_min_reals)
	{
		ss.str("");
		ss << "WARNING: less than " << warn_min_reals << " active realizations...might not be enough";
		message(0, ss.str());
	}
	ph.update(oe, pe);
	message(0, "initial phi summary");
	ph.report();
	ph.write(0, run_mgr_ptr->get_total_runs());
	
	last_best_mean = ph.get_mean(PhiHandler::phiType::COMPOSITE);
	last_best_std = ph.get_std(PhiHandler::phiType::COMPOSITE);
	last_best_lam = pest_scenario.get_pestpp_options().get_ies_init_lam();
	if (last_best_lam <= 0.0)
	{
		double x = last_best_mean / (2.0 * double(oe.shape().second));
		last_best_lam = pow(10.0,(floor(log10(x))));
	}
	
	message(0, "current lambda:", last_best_lam);
	message(0, "initialization complete");
}

void IterEnsembleSmoother::drop_bad_phi(ParameterEnsemble &_pe, ObservationEnsemble &_oe)
{
	assert(_pe.shape().first == _oe.shape().first);
	vector<int> idxs = ph.get_idxs_greater_than(pest_scenario.get_pestpp_options().get_ies_bad_phi(), _oe);
	
	if (idxs.size() > 0)
	{
		message(0, "droppping realizations as bad: ", idxs.size());
		vector<string> par_real_names = pe.get_real_names(), obs_real_names = oe.get_real_names();
		stringstream ss;
		for (auto i : idxs)
			ss << par_real_names[i] << " : " << obs_real_names[i] << " , ";

		message(1, "dropping par:obs realizations: "+ss.str());
		try
		{
			_pe.drop_rows(idxs);
			_oe.drop_rows(idxs);
		}
		catch (const exception &e)
		{
			stringstream ss;
			ss << "drop_bad_phi() error : " << e.what();
			throw_ies_error(ss.str());
		}
		catch (...)
		{
			throw_ies_error(string("drop_bad_phi() error"));
		}
	}
}

void IterEnsembleSmoother::save_mat(string prefix, Eigen::MatrixXd &mat)
{
	stringstream ss;
	ss << iter << '.' << prefix;
	ofstream &f = file_manager.open_ofile_ext(ss.str());
	f << mat << endl;
	f.close();

}


void IterEnsembleSmoother::solve()
{
	iter++;
	stringstream ss;
	ofstream &frec = file_manager.rec_ofstream();
	message(0, "starting solve for iteration:", iter);
	ss << "starting solve for iteration: " << iter;
	performance_log->log_event(ss.str());

	if (oe.shape().first <= error_min_reals)
	{
		message(0, "too few active realizations:", oe.shape().first);
		message(1, "need at least ", error_min_reals);
		throw_ies_error(string("too few active realizations, cannot continue"));
	}
	if (oe.shape().first <= warn_min_reals)
	{
		ss.str("");
		ss << "WARNING: less than " << warn_min_reals << " active realizations...might not be enough";
		message(0, ss.str());
	}

	if ((use_subset) && (subset_size > pe.shape().first))
	{
		ss.str("");
		ss << "++ies_subset size (" << subset_size << ") greater than ensemble size (" << pe.shape().first << ")";
		frec << "  ---  " << ss.str() << endl;
		cout << "  ---  " << ss.str() << endl;
		frec << "  ...reducing ++ies_subset_size to " << pe.shape().first << endl;
		cout << "  ...reducing ++ies_subset_size to " << pe.shape().first << endl;
		subset_size = pe.shape().first;
	}

	double scale = (1.0 / (sqrt(double(oe.shape().first - 1))));
	
	performance_log->log_event("calculate residual matrix");
	//oe_base var_names should be ordered by act_obs_names, so only reorder real_names
	//oe should only include active realizations, so only reorder var_names
	message(1, "calculating residual matrix");
	Eigen::MatrixXd scaled_residual = obscov_inv_sqrt * (oe.get_eigen(vector<string>(), act_obs_names) - 
		oe_base.get_eigen(oe.get_real_names(), vector<string>())).transpose();
	if (verbose_level > 1)
	{
		cout << "scaled_residual: " << scaled_residual.rows() << ',' << scaled_residual.cols() << endl;
		if (verbose_level > 2)
			save_mat("scaled_residual", scaled_residual);
	}
		
	performance_log->log_event("calculate scaled obs diff");
	message(1, "calculating obs diff matrix");
	Eigen::MatrixXd diff = oe.get_eigen_mean_diff(vector<string>(),act_obs_names).transpose();
	Eigen::MatrixXd obs_diff = scale * (obscov_inv_sqrt * diff);
	if (verbose_level > 1)
	{
		cout << "obs_diff: " << obs_diff.rows() << ',' << obs_diff.cols() << endl;
		if (verbose_level > 2)
			save_mat("obs_diff.dat", obs_diff);
	}

	performance_log->log_event("calculate scaled par diff");
	message(1, "calculating par diff matrix");
	pe.transform_ip(ParameterEnsemble::transStatus::NUM);
	diff = pe.get_eigen_mean_diff(vector<string>(), act_par_names).transpose();
	Eigen::MatrixXd par_diff;
	if (pest_scenario.get_pestpp_options().get_ies_use_prior_scaling())
	{
		cout << "...applying prior par cov scaling to par diff matrix" << endl;
		par_diff = scale * *parcov.e_ptr() * diff;
	}
	else
		par_diff = scale * diff;
		
	if (verbose_level > 1)
	{
		cout << "par_diff:" << par_diff.rows() << ',' << par_diff.cols() << endl;
		if (verbose_level > 2)
			save_mat("par_diff.dat", par_diff);
	}

	performance_log->log_event("SVD of obs diff");
	message(1, "calculating SVD of obs diff matrix");
	Eigen::MatrixXd ivec, upgrade_1, s,V,Ut;

	SVD_REDSVD rsvd;
	rsvd.set_performance_log(performance_log);
	rsvd.solve_ip(obs_diff, s, Ut, V, pest_scenario.get_svd_info().eigthresh, pest_scenario.get_svd_info().maxsing);
	Ut.transposeInPlace();
	obs_diff.resize(0, 0);
	
	Eigen::MatrixXd s2 = s.cwiseProduct(s);
	if (verbose_level > 1)
	{
		cout << "s2: " << s2.rows() << ',' << s2.cols() << endl;
		cout << "Ut: " << Ut.rows() << ',' << Ut.cols() << endl;
		cout << "V:" << V.rows() << ',' << V.cols() << endl;
		if (verbose_level > 2)
		{
			save_mat("ut.dat", Ut);
			save_mat("s2.dat", s2);
		}
	}

	vector<ParameterEnsemble> pe_lams;
	vector<double> lam_vals;
	for (auto &lam_mult : lam_mults)
	{
		ss.str("");
		double cur_lam = last_best_lam * lam_mult;
		ss << "starting calcs for lambda" << cur_lam;
		message(1, "starting lambda calcs for lambda", cur_lam);

		performance_log->log_event(ss.str());
		performance_log->log_event("form scaled identity matrix");
		message(1, "calculating scaled identity matrix");
		ivec = ((Eigen::VectorXd::Ones(s2.size()) * (cur_lam + 1.0)) + s2).asDiagonal().inverse();
		if (verbose_level > 1)
		{
			cout << "ivec:" << ivec.rows() << ',' << ivec.cols() << endl;
			if (verbose_level > 2)
				save_mat("ivec.dat", ivec);
		}
		
		
		message(1, "forming X1");
		Eigen::MatrixXd X1 = Ut * scaled_residual;
		if (verbose_level > 1)
		{
			cout << "X1: " << X1.rows() << ',' << X1.cols() << endl;
			if (verbose_level > 2)
				save_mat("X1.dat", X1);
		}
		//scaled_residual.resize(0, 0);
		//Ut.resize(0, 0);
		
		message(1, "forming X2");
		Eigen::MatrixXd X2 = ivec * X1;
		if (verbose_level > 1)
		{
			cout << "X2: " << X2.rows() << ',' << X2.cols() << endl;
			if (verbose_level > 2)
				save_mat("X2.dat", X2);
		}
		X1.resize(0, 0);

		message(1, "forming X3");
		Eigen::MatrixXd X3 = V * s.asDiagonal() * X2;
		if (verbose_level > 1)
		{
			cout << "X3: " << X3.rows() << ',' << X3.cols() << endl;
			if (verbose_level > 2)
				save_mat("X3.dat", X3);
		}
		X2.resize(0, 0);

		message(1, "forming upgrade_1");
		upgrade_1 = -1.0 * par_diff * X3;
		upgrade_1.transposeInPlace();
		if (verbose_level > 1)
		{
			cout << "upgrade_1:" << upgrade_1.rows() << ',' << upgrade_1.cols() << endl;
			if (verbose_level > 2)
				save_mat("upgrade_1.dat", upgrade_1);
		}
		X3.resize(0,0);

		ParameterEnsemble pe_lam = pe;
		pe_lam.set_eigen(*pe_lam.get_eigen_ptr() + upgrade_1);
		upgrade_1.resize(0, 0);
		
		if ((!pest_scenario.get_pestpp_options().get_ies_use_approx()) && (iter > 1))
		{
			performance_log->log_event("calculating parameter correction (full solution)");
			message(1, "calculating parameter correction (full solution, MAP)");
			performance_log->log_event("forming scaled par resid");
			Eigen::MatrixXd scaled_par_resid = pe.get_eigen(vector<string>(), act_par_names) - 
				pe_base.get_eigen(pe.get_real_names(), vector<string>());
			scaled_par_resid.transposeInPlace();
			
			performance_log->log_event("forming x4");
			message(1, "forming X4");
			if (verbose_level > 1)
			{
				cout << "scaled_par_resid: " << scaled_par_resid.rows() << ',' << scaled_par_resid.cols() << endl;
				if (verbose_level > 2)
					save_mat("scaled_par_resid.dat", scaled_par_resid);
			}
			Eigen::MatrixXd x4 = Am.transpose() * scaled_par_resid;
			if (verbose_level > 1)
			{
				cout << "x4: " << x4.rows() << ',' << x4.cols() << endl;
				if (verbose_level > 2)
					save_mat("x4.dat", x4);
			}

			performance_log->log_event("forming x5");
			message(1, "forming X5");
			Eigen::MatrixXd x5 = Am * x4;
			if (verbose_level > 1)
			{
				cout << "x5: " << x5.rows() << ',' << x5.cols() << endl;
				if (verbose_level > 2)
					save_mat("x5.dat", x5);
			}
			
			message(1, "forming X6");
			performance_log->log_event("forming x6");
			Eigen::MatrixXd x6 = par_diff.transpose() * x5;
			if (verbose_level > 1)
			{
				cout << "x6: " << x6.rows() << ',' << x6.cols() << endl;
				if (verbose_level > 2)
					save_mat("x6.dat", x6);
			}
				
			message(1, "forming X7");
			performance_log->log_event("forming x7");
			if (verbose_level > 1)
			{
				cout << "V: " << V.rows() << ',' << V.cols() << endl;
				if (verbose_level > 2)
					save_mat("V.dat", V);
			}	
			Eigen::MatrixXd x7 = V * ivec *V.transpose() * x6;
			if (verbose_level > 1)
			{
				cout << "x7: " << x7.rows() << ',' << x7.cols() << endl;
				if (verbose_level > 2)
					save_mat("x7.dat", x7);
			}

			performance_log->log_event("forming upgrade_2");
			message(1, "forming upgrade_2");
			Eigen::MatrixXd upgrade_2 = -1.0 * (par_diff * x7);
			if (verbose_level > 1)
			{
				cout << "upgrade_2: " << upgrade_2.rows() << ',' << upgrade_2.cols() << endl;
				if (verbose_level > 2)
					save_mat("upgrade_2", upgrade_2);
			}

			pe_lam.set_eigen(*pe_lam.get_eigen_ptr() + upgrade_2.transpose());
		}

		pe_lam.enforce_bounds();

		pe_lams.push_back(pe_lam);
		lam_vals.push_back(cur_lam);
		message(0, "finished calcs for lambda:", cur_lam);

	}
	//return;
	vector<map<int, int>> real_run_ids_lams;
	int best_idx = -1;
	double best_mean = 1.0e+30, best_std = 1.0e+30;
	double mean, std;
	
	message(0, "running lambda ensembles");
	vector<ObservationEnsemble> oe_lams = run_lambda_ensembles(pe_lams, lam_vals);
		
	message(0, "evaluting lambda ensembles");
	message(0, "last mean:", last_best_mean);
	message(0, "last stdev", last_best_std);

	ObservationEnsemble oe_lam_best;
	for (int i=0;i<pe_lams.size();i++)
	{	
		//for testing...
		//pest_scenario.get_pestpp_options_ptr()->set_ies_bad_phi(0.0);
		
		drop_bad_phi(pe_lams[i], oe_lams[i]);
		if (pe_lams[i].shape().first == 0)
		{
			message(1, "all realizations dropped as 'bad' for lambda ", lam_vals[i]);
			continue;
		}
		message(0, "phi summary for lambda:", lam_vals[i]);
		ph.update(oe_lams[i], pe_lams[i]);
		ph.report();
		mean = ph.get_mean(PhiHandler::phiType::COMPOSITE);
		std = ph.get_std(PhiHandler::phiType::COMPOSITE);
		if (mean < best_mean)
		{
			oe_lam_best = oe_lams[i];
			best_mean = mean;
			best_std = std;
			best_idx = i;
		}
	}

	if ((best_idx != -1) && (use_subset) && (subset_size < pe.shape().first))//subset stuff here
	{

		ObservationEnsemble remaining_oe_lam = oe;//copy
		ParameterEnsemble remaining_pe_lam = pe_lams[best_idx];

		vector<int> real_idxs;
		vector<string> temp_idxs,org_pe_idxs,org_oe_idxs;
		int ireal = 0;
		for (int i = subset_size; i < pe.shape().first; i++)
		{
			real_idxs.push_back(i);
			ss.str("");
			ss << ireal;
			temp_idxs.push_back(ss.str());
			ireal++;
		}
		message(0, "running remaining realizations for best lambda:", lam_vals[best_idx]);

		remaining_pe_lam.keep_rows(real_idxs);
		remaining_oe_lam.keep_rows(real_idxs);
		org_pe_idxs = remaining_pe_lam.get_real_names();
		org_oe_idxs = remaining_oe_lam.get_real_names();
		remaining_pe_lam.set_real_names(temp_idxs);
		remaining_oe_lam.set_real_names(temp_idxs);
		vector<int> fails = run_ensemble(remaining_pe_lam, remaining_oe_lam);
		if (fails.size() > 0)
		{
			vector<string> new_pe_idxs, new_oe_idxs;
			vector<int>::iterator start = fails.begin(), end = fails.end();
			for (int i = 0; i < org_pe_idxs.size(); i++)
				if (find(start,end,i) == end)
				{
					new_pe_idxs.push_back(org_pe_idxs[i]);
					new_oe_idxs.push_back(org_oe_idxs[i]);
				}
			remaining_pe_lam.set_real_names(new_pe_idxs);
			remaining_oe_lam.set_real_names(new_oe_idxs);
		}
		else
		{
			remaining_pe_lam.set_real_names(org_pe_idxs);
			remaining_oe_lam.set_real_names(org_oe_idxs);
			
		}
		pe_lams[best_idx].drop_rows(real_idxs); 
		pe_lams[best_idx].append_other_rows(remaining_pe_lam);
		oe_lam_best.append_other_rows(remaining_oe_lam);
		drop_bad_phi(pe_lams[best_idx], oe_lam_best);
		if (oe_lam_best.shape().first == 0)
		{
			throw_ies_error(string("all realization dropped after finishing subset runs...something might be wrong..."));
		}
	}
	if (best_idx == -1)
	{
		message(0, "WARNING:  unsuccessful lambda testing, resetting lambda to 10000.0");
		last_best_lam = 10000.0;
		return;

	}
	ph.update(oe_lam_best, pe_lams[best_idx]);
	best_mean = ph.get_mean(PhiHandler::phiType::COMPOSITE);
	best_std = ph.get_std(PhiHandler::phiType::COMPOSITE);
	if (best_mean < last_best_mean * 1.1)
	{
		message(0,"updating parameter ensemble");
		performance_log->log_event("updating parameter ensemble");
		last_best_mean = best_mean;

		pe = pe_lams[best_idx];
		oe = oe_lam_best;
		
		if (best_std < last_best_std * 1.1)
		{
			double new_lam = lam_vals[best_idx] * 0.75;
			new_lam = (new_lam < lambda_min) ? lambda_min : new_lam;
			message(0, "updating lambda to ", new_lam);
			last_best_lam = new_lam;
		}
		else
		{
			message(0, "not updating lambda");
		}
		last_best_std = best_std;
	}

	else
	{
		message(0, "not updating parameter ensemble");
		ph.update(oe, pe);
		double new_lam = last_best_lam * 10.0;
		new_lam = (new_lam > lambda_max) ? lambda_max : new_lam;
		message(0, "incresing lambda to: ", new_lam);
		last_best_lam = new_lam;
	}
	report_and_save();
}

void IterEnsembleSmoother::report_and_save()
{
	ofstream &frec = file_manager.rec_ofstream();
	frec << endl << "  ---  IterEnsembleSmoother iteration " << iter << " report  ---  " << endl;
	frec << "   number of active realizations:  " << pe.shape().first << endl;
	frec << "   number of model runs:           " << run_mgr_ptr->get_total_runs() << endl;
	
	cout << endl << "  ---  IterEnsembleSmoother iteration " << iter << " report  ---  " << endl;
	cout << "   number of active realizations:   " << pe.shape().first << endl;
	cout << "   number of model runs:            " << run_mgr_ptr->get_total_runs() << endl;

	stringstream ss;
	ss << file_manager.get_base_filename() << "." << iter << ".obs.csv";
	oe.to_csv(ss.str());
	frec << "      current obs ensemble saved to " << ss.str() << endl;
	cout << "      current obs ensemble saved to " << ss.str() << endl;
	ss.str("");
	ss << file_manager.get_base_filename() << "." << iter << ".par.csv";
	pe.to_csv(ss.str());
	frec << "      current par ensemble saved to " << ss.str() << endl;
	cout << "      current par ensemble saved to " << ss.str() << endl;
	
	ph.report();
	ph.write(iter,run_mgr_ptr->get_total_runs());
	
}


vector<ObservationEnsemble> IterEnsembleSmoother::run_lambda_ensembles(vector<ParameterEnsemble> &pe_lams, vector<double> &lam_vals)
{
	stringstream ss;
	ss << "queuing " << pe_lams.size() << " ensembles";
	performance_log->log_event(ss.str());
	run_mgr_ptr->reinitialize();
	vector<int> subset_idxs;
	if ((use_subset) && (subset_size < pe_lams[0].shape().first))
	{
		for (int i = 0; i < subset_size; i++)
			subset_idxs.push_back(i);
	}
	vector<map<int, int>> real_run_ids_vec;
	for (auto &pe_lam : pe_lams)
	{
		try
		{
			real_run_ids_vec.push_back(pe_lam.add_runs(run_mgr_ptr,subset_idxs));
		}
		catch (const exception &e)
		{
			stringstream ss;
			ss << "run_ensemble() error queueing runs: " << e.what();
			throw_ies_error(ss.str());
		}
		catch (...)
		{
			throw_ies_error(string("run_ensembles() error queueing runs"));
		}
	}
	performance_log->log_event("making runs");
	try
	{
		
		run_mgr_ptr->run();
	}
	catch (const exception &e)
	{
		stringstream ss;
		ss << "error running ensembles: " << e.what();
		throw_ies_error(ss.str());
	}
	catch (...)
	{
		throw_ies_error(string("error running ensembles"));
	}

	performance_log->log_event("processing runs");
	vector<int> failed_real_indices;
	vector<ObservationEnsemble> obs_lams;
	ObservationEnsemble _oe = oe;//copy
	if (subset_size < pe_lams[0].shape().first)
		_oe.keep_rows(subset_idxs);
	map<int, int> real_run_ids;
	//for (auto &real_run_ids : real_run_ids_vec)
	for (int i=0;i<pe_lams.size();i++)
	{
		
		real_run_ids = real_run_ids_vec[i];
		try
		{
			failed_real_indices = _oe.update_from_runs(real_run_ids, run_mgr_ptr);
		}
		catch (const exception &e)
		{
			stringstream ss;
			ss << "error processing runs: " << e.what();
			throw_ies_error(ss.str());
		}
		catch (...)
		{
			throw_ies_error(string("error processing runs"));
		}

		if (failed_real_indices.size() > 0)
		{
			stringstream ss;
			vector<string> par_real_names = pe.get_real_names();
			vector<string> obs_real_names = oe.get_real_names();
			ss << "the following par:obs realization runs failed: ";
			for (auto &i : failed_real_indices)
			{
				ss << par_real_names[i] << ":" << obs_real_names[i] << ',';
			}
			performance_log->log_event(ss.str());
			if (failed_real_indices.size() == _oe.shape().first)
			{
				message(0, "WARNING: all realization failed for lambda multiplier :", lam_vals[i]);
				_oe = ObservationEnsemble();
			}
			else
			{
				performance_log->log_event("dropping failed realizations");
				_oe.drop_rows(failed_real_indices);
			}
			
		}
		obs_lams.push_back(_oe);
	}
	return obs_lams;
}


vector<int> IterEnsembleSmoother::run_ensemble(ParameterEnsemble &_pe, ObservationEnsemble &_oe, vector<int> &real_idxs)
{
	stringstream ss;
	ss << "queuing " << _pe.shape().first << " runs";
	performance_log->log_event(ss.str());
	run_mgr_ptr->reinitialize();
	map<int, int> real_run_ids;
	try
	{
		real_run_ids = _pe.add_runs(run_mgr_ptr,real_idxs);
	}
	catch (const exception &e)
	{
		stringstream ss;
		ss << "run_ensemble() error queueing runs: " << e.what();
		throw_ies_error(ss.str());
	}
	catch (...)
	{
		throw_ies_error(string("run_ensemble() error queueing runs"));
	}
	performance_log->log_event("making runs");
	try
	{
		run_mgr_ptr->run();
	}
	catch (const exception &e)
	{
		stringstream ss;
		ss << "error running ensemble: " << e.what();
		throw_ies_error(ss.str());
	}
	catch (...)
	{
		throw_ies_error(string("error running ensemble"));
	}

	performance_log->log_event("processing runs");
	if (real_idxs.size() > 0)
	{
		_oe.keep_rows(real_idxs);
	}
	vector<int> failed_real_indices;
	try
	{
		failed_real_indices = _oe.update_from_runs(real_run_ids,run_mgr_ptr);
	}
	catch (const exception &e)
	{
		stringstream ss;
		ss << "error processing runs: " << e.what();
		throw_ies_error(ss.str());
	}
	catch (...)
	{
		throw_ies_error(string("error processing runs"));
	}

	if (failed_real_indices.size() > 0)
	{
		stringstream ss;
		vector<string> par_real_names = pe.get_real_names();
		vector<string> obs_real_names = oe.get_real_names();
		ss << "the following par:obs realization runs failed: ";
		for (auto &i : failed_real_indices)
		{
			ss << par_real_names[i] << ":" << obs_real_names[i] << ',';
		}
		performance_log->log_event(ss.str());
		message(1, "failed realizations: ", failed_real_indices.size());
		message(1, ss.str());
		performance_log->log_event("dropping failed realizations");
		_pe.drop_rows(failed_real_indices);
		_oe.drop_rows(failed_real_indices);
	}
	return failed_real_indices;
}


void IterEnsembleSmoother::finalize()
{

}
