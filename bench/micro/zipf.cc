#include <vector>
#include <iostream>
#include <stdio.h>
#include <sys/ioctl.h>
#include <limits.h>
#include <assert.h>
#include <boost/random/uniform_real_distribution.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/math/distributions/pareto.hpp>
#include <boost/random.hpp>
#include <algorithm>

using namespace std;
#define B * 1L
#define KB * (1024 B)
#define MB * (1024 KB)
#define GB * (1024 MB)
#define file_size_bytes (4 GB)
#define io_size (4 KB)
#define height 20

#define ALIGN_MASK(x, mask) (((x) + (mask)) & ~(mask))
#define ALIGN(x, a)  ALIGN_MASK((x), ((__typeof__(x))(a) - 1))

void print_dist(const vector<uint64_t> &list) {
   struct winsize w;
   ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

   vector<int> buckets(w.ws_col, 0);
   int upper = *(max_element(list.begin(), list.end()));
   int divisor = upper / (buckets.size() - 1);

   cout << "Bucket divisor: " << divisor << endl;
   for (const auto &i : list) {
      buckets[i/divisor] += 1;
   }
   cout << "first bucket: " << buckets[0] << endl;

   upper = *(max_element(buckets.begin(), buckets.end()));
   divisor = upper / (height - 1);

   for (auto &i : buckets)
      i = i/divisor;

   for (int i = height; i > 0;  i--) {
      for (const auto &j : buckets) {
         if (j >= i)
            cout << "*";
         else
            cout << " ";
      }
      cout << endl;
   }

   for (int i=0; i < w.ws_col; i++)
      cout << "_";
   cout << endl;
}

int main(int argc, char** argv)
{
   vector<uint64_t> io_list;
   boost::mt19937 rg;
   boost::math::pareto_distribution<> dist;
   boost::random::uniform_real_distribution<> uniformReal(1.0,10.0);
   //rg.seed(time(NULL));
   boost::variate_generator<boost::mt19937&, 
   boost::random::uniform_real_distribution<> > generator(rg, uniformReal);

   double value;
   for (uint64_t i = 0; i < file_size_bytes / io_size; i++)  {
      //cout << (int)(file_size_bytes * boost::math::pdf(dist, uniformReal(rg))) << endl;
      value = boost::math::pdf(dist,generator());
      cout << (uint64_t)(file_size_bytes * value) << endl;
      io_list.push_back(ALIGN((uint64_t)(file_size_bytes * value), (4 << 10)));
   }
   //print_dist(io_list);
}
