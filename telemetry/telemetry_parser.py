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


def main(filename: str = '') -> None:
    logging.info(filename)
    telemetry_df = polars.read_csv(filename).sort("Node ID")

    logging.info(telemetry_df.head())
    logging.info(telemetry_df.tail())

    target_columns = ["Success %", "Min Temp (C)", "Max Temp (C)", "Avg Temp (C)", "Std Dev (C)"]
    telemetry_df_mean = telemetry_df.group_by("Node ID").agg([
        polars.col(col)
        .cast(polars.Float64, strict=False)
        .mean()
        .alias(f"{col} Mean")
        for col in target_columns
    ])

    with polars.Config(tbl_rows=-1, tbl_cols=-1, fmt_str_lengths=100):
        logging.info(
            telemetry_df_mean
        )
    telemetry_df_mean_filtered = telemetry_df_mean.filter(polars.col("Success % Mean") != 0)

    with polars.Config(tbl_rows=-1, tbl_cols=-1, fmt_str_lengths=100):
        logging.info(f"\n{telemetry_df_mean_filtered=}")


if __name__ == '__main__':
    start_time: float = time.perf_counter()

    parser = argparse.ArgumentParser(description=r'Boiler plate code to use as a starting '
                                                 r'point and basic environment checkout.',
                                     formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('-ll', '--log_level', default='WARNING',
                        choices=['DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL'],
                        help='changes level of log output, default is WARNING')
    parser.add_argument('-if', '--input_file', help='Input csv file name, required.')

    # Convert args to a dictionary
    args = vars(parser.parse_args(sys.argv[1:]))
    log_level = args['log_level']
    m_filename: str = args['input_file']

    # Changes global level, not best practice but fast
    logging.basicConfig(level=log_level)
    main(m_filename)

    print(f'{os.path.basename(__file__)} execution: {time.perf_counter() - start_time:,.2f}-seconds')
