import argparse
import csv
#import json
from struct import iter_unpack

def average(lst):
    return sum(lst) / len(lst)

def text(field: bytes) -> str:
    octets = field.split(b'\0', 1)[0]
    return octets.decode('cp437')

parser = argparse.ArgumentParser(description='Get some statistics from the Raspa run data binary file')
parser.add_argument('filename', help='File with raspa run data')
parser.add_argument('--csv', help='Export CSV data')
parser.add_argument('--label', help='Plot label')
parser.add_argument('--pdf', help='Export plot to PDF')
parser.add_argument('--plot', help='Plot histogram to screen', action='store_true')

args = parser.parse_args()

filename=args.filename;

with open(filename, 'rb') as fp:
    bindata = fp.read()

data = { 'start': [], 'end': [], 'duration': [] }
data_csv = []

for fields in iter_unpack('<QQ', bindata):
    start, end = fields
    duration = end - start

    if start == 0:
        print('Log underrun detected in file: data may not be reliable!')
    else:
        data['start'].append(start)
        data['end'].append(end)
        data['duration'].append(duration)
        data_csv.append({ 'start': start, 'end': end, 'duration': duration })

duration=data['duration']
t_min=min(duration)
t_max=max(duration)
t_avg=round(average(duration))

print('Execution time: min=' + str(t_min) + ' max=' + str(t_max) + ' avg=' + str(round(t_avg)))

if args.csv:
    csv_columns=['start', 'end', 'duration']
    with open(args.csv, 'w') as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=csv_columns)
        writer.writeheader()
        for item in data_csv:
            writer.writerow(item)

if args.pdf or args.plot:
    import matplotlib.pyplot as plt

    plt.hist(duration, 10, log=True, alpha=0.75)
    if args.label is not None:
        plt.title(args.label)
    comments = ' [min=' + str(t_min) + ', max=' + str(t_max) + ', avg=' + str(t_avg) + ']'
    plt.xlabel('Processing time' + comments)
    plt.ylabel('Number of occurences')
    plt.grid(True)

    if args.pdf:
        plt.savefig(args.pdf)

    if args.plot:
        plt.show()