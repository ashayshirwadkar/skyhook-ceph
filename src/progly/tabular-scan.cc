#include <iostream>
#include <boost/program_options.hpp>
#include "include/rados/librados.hpp"

namespace po = boost::program_options;

#define checkret(r,v) do { \
  if (r != v) { \
    fprintf(stderr, "error %d/%s\n", r, strerror(-r)); \
    assert(0); \
    exit(1); \
  } } while (0)

int main(int argc, char **argv)
{
  uint64_t range_size;
  unsigned num_rows;
  unsigned rows_per_obj;
  double selectivity;
  std::string pool;

  po::options_description gen_opts("General options");
  gen_opts.add_options()
    ("help,h", "show help message")
    ("range-size", po::value<uint64_t>(&range_size)->required(), "data range")
    ("num-rows", po::value<unsigned>(&num_rows)->required(), "number of rows")
    ("rows-per-obj", po::value<unsigned>(&rows_per_obj)->required(), "rows per object")
    ("selectivity", po::value<double>(&selectivity)->required(), "selectivity pct")
		("pool,p", po::value<std::string>(&pool)->required(), "pool")
  ;

  po::options_description all_opts("Allowed options");
  all_opts.add(gen_opts);

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, all_opts), vm);

  if (vm.count("help")) {
    std::cout << all_opts << std::endl;
    return 1;
  }

  po::notify(vm);

  assert(range_size > 0);
  assert(num_rows > 0);
  assert(rows_per_obj > 0);
  assert(num_rows % rows_per_obj == 0);

  assert(selectivity >= 0.0);
  assert(selectivity <= 100.0);
  selectivity /= 100.0;

  // connect to rados
  librados::Rados cluster;
  cluster.init(NULL);
  cluster.conf_read_file(NULL);
  int ret = cluster.connect();
  checkret(ret, 0);

  // open pool i/o context
  librados::IoCtx ioctx;
  ret = cluster.ioctx_create(pool.c_str(), ioctx);
  checkret(ret, 0);

  uint64_t max_val = range_size * selectivity;

  uint64_t total_rows = 0;
  uint64_t filtered_rows = 0;

  const unsigned num_objs = num_rows / rows_per_obj;
  for (unsigned o = 0; o < num_objs; o++) {
    std::stringstream ss;
    ss << "obj." << o;
    const std::string oid = ss.str();

    ceph::bufferlist bl;
    int ret = ioctx.read(oid, bl, 0, 0);
    assert(ret > 0);

    assert(bl.length() / sizeof(uint64_t) == rows_per_obj);
    const uint64_t *rows = (uint64_t*)bl.c_str();
    for (unsigned r = 0; r < rows_per_obj; r++) {
      const uint64_t row_val = rows[r];
      if (row_val < max_val)
        filtered_rows++;
      total_rows++;
    }
  }

  std::cout << "total rows " << total_rows
    << " filtered rows " << filtered_rows
    << " selectivity wanted " << (100.0 * selectivity)
    << " selectivity observed "
    << (100.0 * (double)filtered_rows / (double)total_rows)
    << std::endl;

  ioctx.close();
  cluster.shutdown();

  return 0;
}
