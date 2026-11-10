#include <common/Kernel.hpp>
#include <common/Evaluation.hpp>


int main() {
  kernel::DatabaseMetrics metrics;
  kernel::InMemoryDatabase db(metrics);

  evaluation::populateDataGraph(db);
}