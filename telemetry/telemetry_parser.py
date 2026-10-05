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


def tbd_function(param_str: str) -> None:
    """
    What is this for; what does it do; why does it exist?

    :param:
    :return: None.
    """
    logging.debug(f'This is tbd_function, {param_str}')


def main(filename: str = '') -> None:
    logging.debug(filename)
    tbd_function('foo')


if __name__ == '__main__':
    start_time: float = time.perf_counter()

    parser = argparse.ArgumentParser(description=r'Boiler plate code to use as a starting '
                                                 r'point and basic environment checkout.',
                                     formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('-ll', '--log_level', default='CRITICAL',
                        choices=['DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL'],
                        help='changes level of log output, default is CRITICAL')
    parser.add_argument('-if', '--input_file', help='Input csv file name, required.')

    # Convert args to a dictionary
    args = vars(parser.parse_args(sys.argv[1:]))
    m_log_level = args['log_level']
    m_filename: str = args['input_file']

    logging.basicConfig(level=m_log_level)
    logging.info(args)

    main(m_filename)

    print(f'{os.path.basename(__file__)} execution: {time.perf_counter() - start_time:,.2f}-seconds')
