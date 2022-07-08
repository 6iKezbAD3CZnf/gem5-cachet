from m5.params import *
from m5.SimObject import SimObject

class BaseCtrl(SimObject):
    type = 'BaseCtrl'
    cxx_header = "cachet/base_ctrl.hh"
    cxx_class = 'gem5::BaseCtrl'

    cpu_side_port = ResponsePort("CPU side port")
    mem_side_port = RequestPort("Mem side port")

class SecCtrl(BaseCtrl):
    type = 'SecCtrl'
    cxx_header = "cachet/sec_ctrl.hh"
    cxx_class = 'gem5::SecCtrl'

    read_port = RequestPort("Read port")
    write_port = RequestPort("Write port")

class CTRead(SimObject):
    type = 'CTRead'
    cxx_header = "cachet/ct_read.hh"
    cxx_class = 'gem5::CTRead'

    cpu_side_port = ResponsePort("CPU side port")
    mem_side_port = RequestPort("Memory side port")

class CTWrite(SimObject):
    type = 'CTWrite'
    cxx_header = "cachet/ct_write.hh"
    cxx_class = 'gem5::CTWrite'

    cpu_side_port = ResponsePort("CPU side port")
    mem_side_port = RequestPort("Memory side port")

class MTWrite(SimObject):
    type = 'MTWrite'
    cxx_header = "cachet/mt_write.hh"
    cxx_class = 'gem5::MTWrite'

    cpu_side_port = ResponsePort("CPU side port")
    mem_side_port = RequestPort("Memory side port")
