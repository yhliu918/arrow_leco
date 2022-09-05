import os
import datetime
import re
import pandas as pd
import json
from pathlib import Path
from time import sleep

PROJ_SRC_DIR = '/root/arrow-private/cpp'

src_files = ["normal_200M_uint32.txt", "linear_200M_uint32.txt"]
books = "books_200M_uint32.txt"
fb = "fb-289000.txt"
wiki = "wiki.txt"
newman = "newman.txt"

bitmap_names_dict = {}
# bitmap_names_200M = ["bitmap_random_1e-05_200000000.txt", "bitmap_random_0.0001_200000000.txt", "bitmap_random_0.0005_200000000.txt",
#                      "bitmap_random_0.001_200000000.txt", "bitmap_random_0.01_200000000.txt", "bitmap_random_0.1_200000000.txt"]
sizes = [200000000, 199999994, 289000, 2076000, 233000]
for size in sizes:
    bitmap_names_dict[size] = [f"bitmap_random_1e-05_{size}.txt", f"bitmap_random_0.0001_{size}.txt", f"bitmap_random_0.0005_{size}.txt",
                     f"bitmap_random_0.001_{size}.txt", f"bitmap_random_0.01_{size}.txt", f"bitmap_random_0.1_{size}.txt"]
selectivities = [0.001, 0.01, 0.05, 0.1, 1, 10]
# encodings = ['LECO']
encodings = ['PLAIN', 'DICT','FOR', 'LECO']
block_size_list = [2000, 200, 289, 2076, 233]

def parse_output(filename, output_stats):
    # os.makedirs(os.path.dirname("outputs/stats.json"), exist_ok=True)
    stats = open(filename, 'a+')
    stats.write(json.dumps(output_stats)+"\n")
    stats.close()


def collect_results(filename):
    data = []
    # filename = "outputs/stats.json"
    # os.makedirs(os.path.dirname(filename), exist_ok=True)
    f = open(filename)
    for line in f:
        data.append(json.loads(line.strip()))
    df = pd.DataFrame(data=data)
    df.to_csv(filename.split('.')[0] + '.csv', index=False)

def compile_binary(block_size):
    # Read in the file
    with open(f'{PROJ_SRC_DIR}/src/parquet/encoding.cc', 'r') as file :
        filedata = file.read()
        file.close()

    replacement = f'constexpr size_t kForBlockSize = {block_size};'
    # Replace the target string
    filedata = re.sub('constexpr size_t kForBlockSize = [0-9]+;', replacement, filedata)
    # filedata = filedata.replace('constexpr size_t kForBlockSize = 200;', replacement)

    # Write the file out again
    with open(f'{PROJ_SRC_DIR}/src/parquet/encoding.cc', 'w') as file:
        file.write(filedata)
        file.close()
    # Compile the file
    os.system(f'/usr/bin/cmake --build {PROJ_SRC_DIR}/out/build/leco-release --target for')
    os.system(f'cd {PROJ_SRC_DIR}/out/build/leco-release/release/; mv for for_{block_size}')

def gen_data():
    for encoding in encodings:
        for src in src_files:
            os.system(f'''{PROJ_SRC_DIR}/out/build/leco-release/release/for_2000 \
                {encoding} 1 {src} {bitmap_names_dict[200000000][0]} 1''')
        os.system(f'''{PROJ_SRC_DIR}/out/build/leco-release/release/for_200 \
                {encoding} 1 {books} {bitmap_names_dict[200000000][0]} 1''')
        os.system(f'''{PROJ_SRC_DIR}/out/build/leco-release/release/for_289 \
                {encoding} 1 {fb} {bitmap_names_dict[200000000][0]} 1''')
        os.system(f'''{PROJ_SRC_DIR}/out/build/leco-release/release/for_2076 \
                {encoding} 1 {wiki} {bitmap_names_dict[200000000][0]} 1''')
        os.system(f'''{PROJ_SRC_DIR}/out/build/leco-release/release/for_233 \
                {encoding} 1 {newman} {bitmap_names_dict[200000000][0]} 1''')


def exp_file_unit(output_file_name, output_stats, encoding, dataset, size, block_size):
    # output_file.write(f"{dataset}\n")
    output_stats['dataset'] = dataset
    for i, bitmap in enumerate(bitmap_names_dict[size]):
        output_stats['selectivity'] = selectivities[i]
        os.system('sync; echo 3 > /proc/sys/vm/drop_caches')
        output = os.popen(f'''{PROJ_SRC_DIR}/out/build/leco-release/release/for_{block_size} \
        {encoding} 1 {dataset} {bitmap} 0''').read()
        for line in output.splitlines():
            if line.startswith("pure scan"):
                # output_file.write(line.split(':')[-1].strip()+"\n")
                output_stats['query_time'] = line.split(':')[-1].strip()
                parse_output(output_file_name, output_stats)
                break
        sleep(1)

def run_exp():
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    # output_file = open(timestamp+".json", "a+")
    out_file_name = timestamp+".json"
    output_stats = {}

    for i in range(5):
        output_stats['i'] = i
        for encoding in encodings:
            print(f'running exp for {encoding}')
            output_stats['encoding'] = encoding
            # output_file.write(f"{encoding}\n")
            # linear and normal, 200M bitmap, 2000 block size
            for src in src_files:
                # output_file.write(f"{src}\n")
                output_stats['dataset'] = src
                for i, bitmap in enumerate(bitmap_names_dict[200000000]):
                    output_stats['selectivity'] = selectivities[i]
                    os.system('sync; echo 3 > /proc/sys/vm/drop_caches')
                    output = os.popen(f'''{PROJ_SRC_DIR}/out/build/leco-release/release/for_2000 \
                    {encoding} 1 {src} {bitmap} 0''').read()
                    for line in output.splitlines():
                        if line.startswith("pure scan"):
                            # output_file.write(line.split(':')[-1].strip()+"\n")
                            output_stats['query_time'] = line.split(':')[-1].strip()
                            parse_output(out_file_name, output_stats)
                            break
                    sleep(1)
            # books, 199999994 bitmap, 200 block size
            exp_file_unit(out_file_name, output_stats, encoding, books, 199999994, 200)
            # fb, 289K bitmap, 289 block size
            exp_file_unit(out_file_name, output_stats, encoding, fb, 289000, 289)
            # wiki, 2076K bitmap, 2076 block size
            exp_file_unit(out_file_name, output_stats, encoding, wiki, 2076000, 2076)
            # newman, 233K bitmap, 233 block size
            exp_file_unit(out_file_name, output_stats, encoding, newman, 233000, 233)
    # print(f'output saved to: {timestamp}.txt')
    # output_file.close()
    collect_results(out_file_name)

# main
if __name__ == "__main__":
    os.chdir(f"{PROJ_SRC_DIR}/out/build/leco-release/release")

    # for block_size in block_size_list:
    #     compile_binary(block_size)

    # gen_data()
    print('finished gen data')
    run_exp()