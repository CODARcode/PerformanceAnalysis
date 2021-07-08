#include <pybind11/pybind11.h>
#include <sim.hpp>

using namespace chimbuko;
using namespace chimbuko_sim;

namespace py = pybind11;

PYBIND11_MODULE(ad_params, m) {
  m.doc() = "PYBIND11 module for ad_params in Chimbuko Simulator";

  m.def("adAlgorithmParams", &adAlgorithmParams, "Set the parameters of the ad algorithm");
  
  m.def("globalParams", &globalParams, "If an AD algorithm is not used (ad_algorithm() == none [default]) this is the object containing the params that are placed in the provDB");

  py::class_<ADalgParams>(m, "ADalgParams")
    .def(py::init<>())
    .def(py::init<const std::string &, double, bool, double, ADOutlier::OutlierStatistic>());
}

PYBIND11_MODULE(ad, m) {
  m.doc() = "PYBIND11 module for ad_sim in Chimbuko Simulator";

  py::class_<ADsim>(m, "ADsim")
    .def(py::init<int, int, int, unsigned long, unsigned long>())
    .def(py::init<>())
    .def("getProvDBclient", &ADsim::getProvDBclient, py::return_value_policy::reference)
    .def("addExec", &ADsim::addExec)
    .def("attachCounter", &ADsim::attachCounter)
    .def("attachComm", &ADsim::attachComm)
    .def("registerGPUthread", &ADsim::registerGPUthread)
    .def("bindCPUparentGPUkernel", &ADsim::bindCPUparentGPUkernel)
    .def("bindParentChild", &ADsim::bindParentChild)
    .def("step", &ADsim::step)
    .def("largest_step", &ADsim::largest_step)
    .def("nStepExecs", &ADsim::nStepExecs);
}
    
PYBIND11_MODULE(provdb, m) {
  
  m.doc() = "PYBIND11 modul for provdb in Chimbuko Simulator";
  
  m.def("nshards", &nshards, py::return_value_policy::copy);

  m.def("getProvDB", &getProvDB);

  py::class_<provDBsim>(m, "provDBsim")
    .def(py::init<int>())
    .def("getAddr", &provDBsim::getAddr, py::return_value_policy::copy)
    .def("getNshards", &provDBsim::getNshards);
  
}


