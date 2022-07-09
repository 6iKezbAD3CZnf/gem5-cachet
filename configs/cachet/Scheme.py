import m5.objects
from m5.objects import *
from MetaCache import MetaCache

def CTConfig(i, system, xbar, mem_ctrls):
    # Insert the security controllers between
    # the memory controllers and the membus
    system.sec_ctrl = SecCtrl()
    system.read_ctrl = CTRead()
    system.write_ctrl = CTWrite()
    system.meta_cache = MetaCache()
    system.meta_bus = SystemXBar()
    system.mem_bus = SystemXBar()

    xbar.mem_side_ports = system.sec_ctrl.cpu_side_port
    system.sec_ctrl.read_port = \
            system.read_ctrl.cpu_side_port
    system.sec_ctrl.write_port = \
            system.write_ctrl.cpu_side_port
    system.meta_bus.cpu_side_ports = [
            system.read_ctrl.mem_side_port
            ]
    system.meta_bus.mem_side_ports = \
            system.meta_cache.cpu_side
    system.mem_bus.cpu_side_ports = [
            system.write_ctrl.mem_side_port,
            system.meta_cache.mem_side,
            system.sec_ctrl.mem_side_port
            ]
    mem_ctrls[i].port = system.mem_bus.mem_side_ports

def MTConfig(i, system, xbar, mem_ctrls):
    # Insert the security controllers between
    # the memory controllers and the membus
    system.sec_ctrl = SecCtrl()
    system.read_ctrl = CTRead()
    system.write_ctrl = MTWrite()
    system.meta_cache = MetaCache()
    system.meta_bus = SystemXBar()
    system.mem_bus = SystemXBar()

    xbar.mem_side_ports = system.sec_ctrl.cpu_side_port
    system.sec_ctrl.read_port = \
            system.read_ctrl.cpu_side_port
    system.sec_ctrl.write_port = \
            system.write_ctrl.cpu_side_port
    system.meta_bus.cpu_side_ports = [
            system.write_ctrl.mem_side_port,
            system.read_ctrl.mem_side_port
            ]
    system.meta_bus.mem_side_ports = \
            system.meta_cache.cpu_side
    system.mem_bus.cpu_side_ports = [
            system.write_ctrl.mem_bypass_port,
            system.meta_cache.mem_side,
            system.sec_ctrl.mem_side_port
            ]
    mem_ctrls[i].port = system.mem_bus.mem_side_ports
