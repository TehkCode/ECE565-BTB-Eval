# Simple test script
#
# Alpha: "m5 test.py"
# MIPS: "m5 test.py -a Mips -c hello_mips"

import os, optparse, sys

import m5
from m5.objects import *
from FullO3Config import *

# parse command-line arguments
parser = optparse.OptionParser()

parser.add_option("-c", "--cmd", default="hello",
        help="The binary to run in syscall emulation mode.")
parser.add_option("-o", "--options", default="",
        help="The options to pass to the binary, use \" \" around the entire\
                string.")
parser.add_option("-i", "--input", default="",
        help="A file of input to give to the binary.")
parser.add_option("-t", "--timing", action="store_true",
        help="Use simple timing CPU.")
parser.add_option("-d", "--detailed", action="store_true",
        help="Use detailed CPU.")
parser.add_option("-m", "--maxtick", type="int",
        help="Set the maximum number of ticks to run  for")

(options, args) = parser.parse_args()

if args:
    print "Error: script doesn't take any positional arguments"
    sys.exit(1)

# build configuration
this_dir = os.path.dirname(__file__)

process = LiveProcess()
process.executable = os.path.join(this_dir, options.cmd)
process.cmd = options.cmd + " " + options.options
if options.input != "":
    process.input = options.input

magicbus = Bus()
mem = PhysicalMemory()

if options.timing and options.detailed:
       print "Error: you may only specify one cpu model";
       sys.exit(1)

if options.timing:
    cpu = TimingSimpleCPU()
elif options.detailed:
    #check for SMT workload
    workloads = options.cmd.split(';')
    if len(workloads) > 1:
        process = []
        smt_idx = 0
        inputs = []

        if options.input != "":
            inputs = options.input.split(';')

        for wrkld in workloads:
            smt_process = LiveProcess()
            smt_process.executable = os.path.join(this_dir, wrkld)
            smt_process.cmd = wrkld + " " + options.options
            if inputs and inputs[smt_idx]:
                smt_process.input = inputs[smt_idx]
            process += [smt_process, ]
            smt_idx += 1

    cpu = DetailedO3CPU()
else:
    cpu = AtomicSimpleCPU()
cpu.workload = process
cpu.mem = magicbus
cpu.icache_port=magicbus.port
cpu.dcache_port=magicbus.port

system = System(physmem = mem, cpu = cpu)

if options.timing or options.detailed:
    system.mem_mode = 'timing'


mem.port = magicbus.port
root = Root(system = system)

# instantiate configuration
m5.instantiate(root)

# simulate until program terminates
if options.maxtick:
    exit_event = m5.simulate(options.maxtick)
else:
    exit_event = m5.simulate()

print 'Exiting @ tick', m5.curTick(), 'because', exit_event.getCause()

