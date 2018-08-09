
#include <iostream>
#include <functional>
#include <map>
#include <vector>
#include <thread>
#include <mutex>
#include <Eigen/Dense>
#include <deque>


//class declaration
class Worker
{
public:
	Worker(Eigen::MatrixXd &_mat, std::map<int, std::vector<int>> &_cases, std::map<int,Eigen::MatrixXd> &_results);
	Eigen::MatrixXd calc(int col_idx, std::vector<int> row_idxs);
	std::pair<int, std::vector<int>> get_next();

private:
	Eigen::MatrixXd &mat; //by ref so shared between all threads
	std::map<int, std::vector<int>> &cases; // by ref - shared between all threads
	std::map<int, Eigen::MatrixXd> &results;
	Eigen::MatrixXd extract(int col_idx, std::vector<int> row_idxs);
	void put(int col_idx,Eigen::MatrixXd &matrix);
	std::mutex next_lock, extract_lock, put_lock;
};


//class construtor
Worker::Worker(Eigen::MatrixXd &_mat, std::map<int, std::vector<int>> &_cases, std::map<int,Eigen::MatrixXd> &_results):
	mat(_mat), cases(_cases), results(_results)
{

}


//do some calculations on a subset of the whole matrix
Eigen::MatrixXd Worker::calc(int col_idx, std::vector<int> row_idxs)
{	
	Eigen::MatrixXd extract_mat = extract(col_idx, row_idxs);
	//some operation
	Eigen::MatrixXd noise = Eigen::MatrixXd::Random(100,100);
	
	extract_mat = extract_mat.array() + 1.0;
	
	Eigen::JacobiSVD<Eigen::MatrixXd> svd(noise, Eigen::ComputeThinU | Eigen::ComputeThinV);
	Eigen::MatrixXd sv = svd.singularValues();

	put(col_idx, sv);
	return extract_mat;
}

//thread safe: put the results into the container
void Worker::put(int col_idx,Eigen::MatrixXd &matrix)
{
	std::lock_guard<std::mutex> g(put_lock);
	if (results.find(col_idx) != results.end())
	{
		std::cout << "shits busted" << std::endl;
	}
	results[col_idx] = matrix;
	return;

}

//thread safe: extract the piece we need
Eigen::MatrixXd Worker::extract(int col_idx, std::vector<int> row_idxs)
{
	std::lock_guard<std::mutex> g(extract_lock);
	Eigen::MatrixXd extract_mat(row_idxs.size(), 1);
	int i = 0;
	for (auto ri : row_idxs)
	{
		extract_mat(i, 0) = mat(ri, col_idx);
		++i;
	}
	return extract_mat;
}


//thread safe: get the next par to process
std::pair<int, std::vector<int>> Worker::get_next()
{
	std::lock_guard<std::mutex> g(next_lock);
	//the end condition
	if (cases.size() == 0)
	{
		std::pair<int, std::vector<int>> p(-999,std::vector<int>());
		return p;
	}
	else
	{
		std::map<int, std::vector<int>>::iterator it = cases.begin();
		std::pair<int, std::vector<int>> p(it->first, it->second);
		cases.erase(it);
		return p;
	}
}

//threaded function
void worker_calc(int id, Worker &worker)
{
	while (true)
	{
		std::pair<int, std::vector<int>> p = worker.get_next();
		if ((p.first < 0) || (p.second.size() == 0))
			//no more work to do, so return
			break;
		worker.calc(p.first, p.second);
		//not threaded so output can be all jacked up
		std::cout << "id: " << id << "case: " << p.first << std::endl;
	}
}


//main function
int main()
{
	// matrix dimensions
	int n = 10000;
	
	Eigen::MatrixXd mat(n, n);
	mat.setOnes();

	//build up some fake tasks to do 
	std::map<int, std::vector<int>> cases;
	for (int i = 1; i < n;i++)
	{
		std::vector<int> idxs;
		for (int ii = 0; ii < i; ii++)
			idxs.push_back(ii);
		cases[i] = idxs;
	}
	std::map<int, Eigen::MatrixXd> results;
	//the worker with the appropriate thread machinery
	Worker worker(mat,cases, results);

	//start the threads
	std::vector<std::thread> threads;
	int num_threads = 1;
	for (int i = 0; i < num_threads; i++)	
		threads.push_back(std::thread(worker_calc, i, std::ref(worker)));
	
	//join the threads back - this waits until all threads are done
	for (auto &t : threads)
		t.join();

	//check that the results are correct
	/*for (auto r : results)
	{
		std::cout << r.first << ", " << r.second.size() << std::endl;
	}*/
    return 0;
}

