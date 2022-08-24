import os
import datetime
from pathlib import Path
from time import sleep

src_files = ["normal_200M_uint32.txt", "linear_200M_uint32.txt"]
bitmap_names_200M = ["bitmap_random_1e-05_200000000.txt", "bitmap_random_0.0001_200000000.txt", "bitmap_random_0.0005_200000000.txt",
                     "bitmap_random_0.001_200000000.txt", "bitmap_random_0.01_200000000.txt", "bitmap_random_0.1_200000000.txt"]
# encodings = ['PLAIN']
encodings = ['FOR', 'LECO']
timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
output_file = open(timestamp+".txt", "w")

for encoding in encodings:
    output_file.write(f"{encoding}\n")
    for src in src_files:
        for bitmap in bitmap_names_200M:
            os.system('sync; echo 3 > /proc/sys/vm/drop_caches')
            output = os.popen(f'''/root/arrow-private/cpp/out/build/leco-release/release/for \
            {encoding} 1 {src} {bitmap} 0''').read()
            for line in output.splitlines():
                if line.startswith("pure scan"):
                    output_file.write(line.split(':')[-1].strip()+"\n")
                    break
            sleep(1)
output_file.close()