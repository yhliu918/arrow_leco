import duckdb
import time

begin = time.time()
duckdb.query('''SELECT * FROM 'Anunciante_dict_snappy.parquet' ''').fetchall()
print("query time (s):", time.time() - begin)
