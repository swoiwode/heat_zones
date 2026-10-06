r"""
boilerplate.py - basic template & some notes

Get the current stock (INTC) price, use the data from Fidelity and
display something meaningful to the screen
Scott Woiwode, 2024/09/13

pip freeze > requirements.txt

py -0p
    -V:3.13[-64] *   C:\Users\Scott\AppData\Local\Python\pythoncore-3.13-64\python.exe
    -V:3.12          C:\Users\Scott\AppData\Local\Microsoft\WindowsApps\
                     PythonSoftwareFoundation.Python.3.12_qbz5n2kfra8p0\python.exe

py -3.13 -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt

pyuic6.exe <ui file> -o <py file)

pyinstaller.exe --onefile <py or pyw file>
.pyw extension will prevent the compiled exe from opening a blank command window at run time.

Scott Woiwode, 2026/05/28
"""
import argparse
import logging
import sys
import os
import time

import polars
import matplotlib.pyplot


def main(filename: str = '') -> None:
    logging.info(filename)
    target_columns = ["Node ID", "Success %", "Min Temp (C)", "Max Temp (C)", "Avg Temp (C)", "Std Dev (C)"]

    telemetry_df = (
        polars.scan_csv(filename)
        .filter(polars.col("Success %") != 0)
        .select(target_columns)
        .sort("Node ID")
        .collect()
    )

    with polars.Config(tbl_rows=-1, tbl_cols=-1, fmt_str_lengths=100):
        logging.info(
            telemetry_df
        )
    node_id_list = telemetry_df["Node ID"].unique().sort().to_list()

    found_fliers = []
    node_count = []
    bxp_stats = []
    for count, node_name in enumerate(node_id_list):
        node_df = telemetry_df.filter(telemetry_df["Node ID"] == node_name)
        avg = node_df.select(polars.col("Avg Temp (C)").cast(polars.Float64).mean()).item()
        std = node_df.select(polars.col("Std Dev (C)").cast(polars.Float64).std()).item()
        min_avg = node_df.select(polars.col("Min Temp (C)").cast(polars.Float64).mean()).item()
        max_avg = node_df.select(polars.col("Max Temp (C)").cast(polars.Float64).mean()).item()
        found_fliers = (
            node_df.filter(
                polars.col("Min Temp (C)")
                .cast(polars.Float64) < min_avg)
            .get_column("Min Temp (C)")
            .to_list()
        )
        found_fliers = found_fliers + (
            node_df.filter(
                polars.col("Max Temp (C)")
                .cast(polars.Float64) > max_avg)
            .get_column("Max Temp (C)")
            .to_list()
        )
        stats = {
            'label': node_name,
            'med': avg,
            'q1': avg - std,
            'q3': avg + std,
            'whislo': min_avg,
            'whishi': max_avg,
            'fliers': []
        }
        node_count.append(count + 1)
        bxp_stats.append(stats)
        found_fliers = []
    print(node_count)
    print(bxp_stats)

    graphfigure, axisleft = matplotlib.pyplot.subplots(figsize=(10, 7.5))
    axisleft.bxp(bxp_stats, node_count)

    axisleft.set_title('Node Temperature Statistics')

    matplotlib.pyplot.tight_layout()
    matplotlib.pyplot.show()


if __name__ == '__main__':
    start_time: float = time.perf_counter()

    parser = argparse.ArgumentParser(description=r'Boiler plate code to use as a starting '
                                                 r'point and basic environment checkout.',
                                     formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('-ll', '--log_level', default='WARNING',
                        choices=['DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL'],
                        help='changes level of log output, default is WARNING')
    parser.add_argument('-if', '--input_file', required=True,
                        help='Input csv file name, required.')

    # Convert args to a dictionary
    args = vars(parser.parse_args(sys.argv[1:]))
    log_level = args['log_level']
    m_filename: str = args['input_file']

    # Changes global level, not best practice but fast
    logging.basicConfig(level=log_level)
    main(m_filename)

    print(f'{os.path.basename(__file__)} execution: {time.perf_counter() - start_time:,.2f}-seconds')
