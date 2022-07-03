from m5.params import *
from m5.SimObject import SimObject

class CTRead(SimObject):
    type = 'CTRead'
    cxx_header = "cachet/ct_read.hh"
    cxx_class = 'gem5::CTRead'

    cpu_side_port = ResponsePort("CPU side port")
    mem_side_port = RequestPort("Memory side port")
