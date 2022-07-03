from m5.params import *
from m5.SimObject import SimObject

class SecCtrl(SimObject):
    type = 'SecCtrl'
    cxx_header = "cachet/sec_ctrl.hh"
    cxx_class = 'gem5::SecCtrl'

    cpu_side_port = ResponsePort("CPU side port")
    mem_port = RequestPort("Mem port")
    read_port = RequestPort("Read port")
    write_port = RequestPort("Write port")
