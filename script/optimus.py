import csv
import subprocess
import argparse
import time
import re
import math

# c - 2^^c total size
# b - 2^^b bytes per block
# s - 2^^s number of blocks per set
# v - 2^^v number of blocks for the victim cache
# t - fetch policy: (B)locking (S)ubblocking
# r - replacement policy: (L)RU (N)MRU-FIFO
def main(cmd, trace, output):
	fetch_policies = ['B', 'S']
	replace_policies = ['L', 'N']
	# brutas!
	for t in fetch_policies:
		for r in replace_policies:
			for c in range(15, 16):
				for b in range(6, 14):
					for s in range(0, c - b + 1):
						if s >= c - b:
							continue
						for v in range(0, s + 1): # v should <= s
							writeResult(c, b, s, v, t, r, cmd, trace, output)


def writeResult(c, b, s, v, t, r, cmd, trace, output):
	opts = ['-c', '-b', '-s', '-v', '-t', '-r']
	params = [c, b, s, v, t, r]
	arg = buildArgs(opts, params)
	proc = subprocess.Popen([cmd] + arg, stdout=subprocess.PIPE, stdin=subprocess.PIPE)
	with open(trace, 'r') as f:
		result = proc.communicate(input=f.read())[0]
		# print result
		aat = re.search('\(AAT\):\s(\d*\.\d*)', result).group(1)
		overhead = re.search('Overhead:\s(\d*)', result).group(1)
		ratio = re.search('Ratio:\s(\d*\.\d*)', result).group(1)
		totalSize = int(math.ceil((float(overhead)/float(ratio) + float(overhead))/8))
		print 'AAT: ' + aat
		print 'total size:', totalSize, 'Bytes'
		with open(output, 'a') as csvf:
			writer = csv.writer(csvf)
			row = params + [aat, totalSize]
			writer.writerow(row)

def buildArgs(opts, args):
	result = []
	if (len(opts)!=len(args)):
		raise Exception('error, len opts and len args not equal')
	for i in range(0, len(args)):
		result.append(opts[i])
		result.append(str(args[i]))
	return result

if __name__ == '__main__':
	parser = argparse.ArgumentParser(description='script')
	parser.add_argument('--trace', dest='trace', help='trace path')
	parser.add_argument('--cmd', dest='cmd', help='cmd path')
	parser.add_argument('--output', dest='output', help='output path')
	args = parser.parse_args()
	if not args.trace:
		args.trace = '../traces/perlbench.trace'
	if not args.cmd:
		args.cmd = '../cachesim'
	if not args.output:
		args.output = 'output.csv'
	print 'running'
	start = time.time()
	main(args.cmd, args.trace, args.output)
	end = time.time()
	print 'elapsed time: ', round(end - start, 3), 'seconds'
