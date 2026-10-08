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
import time
import argparse
import os
import sys
import logging

import matplotlib.pyplot
import polars


def mean_node_value(df: polars.DataFrame, id_node: str, id_column: str) -> float:
    return (
        df.filter(polars.col("Node ID") == id_node)
        .select(polars.col(id_column).cast(polars.Float64).mean())
        .item()
    )


def std_node_value(df: polars.DataFrame, id_node: str, id_column: str) -> float:
    return (
        df.filter(polars.col("Node ID") == id_node)
        .select(polars.col(id_column).cast(polars.Float64).std())
        .item()
    )


def main(filename: str = '') -> None:
    logging.info(filename)
    target_columns = ["Node ID", "Success %", "Min Temp (C)", "Max Temp (C)", "Avg Temp (C)"]

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

    node_id = []
    min_mean = []
    min_std = []
    mean_mean = []
    mean_std = []
    max_mean = []
    max_std = []
    min_error = []
    max_error = []
    for node in telemetry_df["Node ID"].unique().sort().to_list():
        node_id.append(node)

        min_mean_value = mean_node_value(df=telemetry_df, id_node=node, id_column="Min Temp (C)")
        min_mean.append(min_mean_value)
        min_std_value = std_node_value(df=telemetry_df, id_node=node, id_column="Min Temp (C)")
        min_std.append(min_std_value)

        mean_mean_value = mean_node_value(df=telemetry_df, id_node=node, id_column="Avg Temp (C)")
        mean_mean.append(mean_mean_value)
        mean_std_value = std_node_value(df=telemetry_df, id_node=node, id_column="Avg Temp (C)")
        mean_std.append(mean_std_value)

        max_mean_value = mean_node_value(df=telemetry_df, id_node=node, id_column="Max Temp (C)")
        max_mean.append(max_mean_value)
        max_std_value = std_node_value(df=telemetry_df, id_node=node, id_column="Max Temp (C)")
        max_std.append(max_std_value)

        min_error.append(mean_mean_value - min_mean_value)
        max_error.append(max_mean_value - mean_mean_value)

    telemetry_df_node_stats = polars.DataFrame({
        'node id': node_id,
        'min mean': min_mean,
        'min std': min_std,
        'min error': min_error,
        'mean': mean_mean,
        'mean std': mean_std,
        'max mean': max_mean,
        'max std': max_std,
        'max error': max_error,
    })

    with polars.Config(tbl_rows=-1, tbl_cols=-1, fmt_str_lengths=100):
        logging.info(
            telemetry_df_node_stats
        )

    telemetry_df_node_stats = telemetry_df_node_stats.with_columns([
        (polars.col("min std") / polars.col("min mean")).alias("min mean cv"),
        (polars.col("mean std") / polars.col("mean")).alias("mean cv"),
        (polars.col("max std") / polars.col("max mean")).alias("max mean cv"),
    ])

    # Matplotlib expects a 2xN array/list for asymmetric errors
    asymmetric_error = [telemetry_df_node_stats['min error'], telemetry_df_node_stats['max error']]

    graphfigure, axisleft = matplotlib.pyplot.subplots(figsize=(10, 7.5))

    # Plot the error bars
    axisleft.errorbar(
        x=telemetry_df_node_stats['node id'],
        y=telemetry_df_node_stats['mean'],
        yerr=asymmetric_error,
        fmt='D',
        color='black',
        ecolor='black',
        elinewidth=1,
        capsize=10,
        label='Mean with High/Low Range'
    )

    x_indices = list(range(len(telemetry_df_node_stats)))
    # --- 🟢 DYNAMIC MEASUREMENT LAYER ---
    # Force Matplotlib to calculate layout right now so we can convert data units to screen points
    graphfigure.canvas.draw()

    # Calculate exactly how many screen points equal 1 unit on the Y-axis
    zero_pos = axisleft.transData.transform((0, 0))
    one_pos = axisleft.transData.transform((0, 1))
    pixels_per_y_unit = abs(one_pos[1] - zero_pos[1])
    points_per_y_unit = pixels_per_y_unit * (72 / graphfigure.dpi)

    # --- TRUE ROUND CIRCLES (Locked to Y-Axis Diameter) ---
    # Matplotlib scatter 's' parameter requires the bounding box area (Diameter in points squared)
    max_sizes = (telemetry_df_node_stats['max mean cv'] * points_per_y_unit) ** 2
    axisleft.scatter(x_indices, telemetry_df_node_stats['max mean'], s=max_sizes, color='black',
                     alpha=0.25, edgecolors='white', linewidths=1, zorder=3)

    mean_sizes = (telemetry_df_node_stats['mean cv'] * points_per_y_unit) ** 2
    axisleft.scatter(x_indices, telemetry_df_node_stats['mean'], s=mean_sizes, color='black',
                     alpha=0.25, edgecolors='white', linewidths=1, zorder=3)

    min_sizes = (telemetry_df_node_stats['min mean cv'] * points_per_y_unit) ** 2
    axisleft.scatter(x_indices, telemetry_df_node_stats['min mean'], s=min_sizes, color='black',
                     alpha=0.25, edgecolors='white', linewidths=1.5, zorder=3)

    axisleft.set_title('Nodes - high/mean/low\n'
                       'gray circles, data CV')
    axisleft.set_ylabel('Temperature (°C)')
    axisleft.tick_params(axis='x', labelrotation=90)
    axisleft.grid(axis='y', linestyle='--', alpha=0.5)
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
