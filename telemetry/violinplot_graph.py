import matplotlib.pyplot
import pandas
import argparse
import time
import os
import sys
import datetime

start_time = time.perf_counter()

if __name__ == '__main__':
    operating_system = os.name

    today_year = datetime.date.today().year
    today_month = datetime.date.today().month
    today_day = datetime.date.today().day
    today_date = f'{today_month:02d}/{today_day:02d}/{today_year}'
    file_date = f'{today_month:02d}{today_day:02d}{today_year}'

    # print(today_date, file_date)

    parser = argparse.ArgumentParser(description=r"Read CSV file and save violinplot png graphs. "
                                                 r"Expects and uses 'health_data.csv' as a default.",
                                     formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='enables print message output, default is False')
    parser.add_argument('-sg', '--showgraph', action='store_true',
                        help='show graph on screen, default is False')
    parser.add_argument('-fn', '--filename', default='health_data.csv',
                        help=".csv file name, default is 'health_data.csv'")
    parser.add_argument('-vt', '--violinplottitle', default=None,
                        help="add text to violinplot graph title, default is None")
    parser.add_argument('-sd', '--sevenday', action='store_true',
                        help="create graph of the last complete 7-days, default is False")

    # Convert args to a dictionary
    args = vars(parser.parse_args(sys.argv[1:]))
    verbose = args['verbose']
    show_graph = args['showgraph']
    file_name = args['filename']
    violinplot_title = args['violinplottitle']
    seven_day = args['sevenday']

    if verbose:
        print(args)

    # linestyle - ‘-‘ solid, ‘:’ dotted, ‘—‘ dashed.
    # marker - ‘o’ circle, ‘+’ plus, ‘.’ point.
    # Increasing linewidth and markersize can be used for emphasis, alpha for de-emphasis.
    plot_colors = {0: 'dodgerblue', 1: 'darkorange', 2: 'olivedrab', 3: 'goldenrod', 4: 'plum',
                   5: 'skyblue', 6: 'sandybrown', 7: 'yellowgreen', 8: 'gold', 9: 'thistle'}

    pandas.set_option('display.width', 320)
    pandas.set_option('display.max_columns', 25)

    health_data_df = pandas.read_csv(file_name)
    if verbose:
        print(f'File read complete: {file_name}')

    health_data_df_rows = health_data_df.shape[0]
    if health_data_df_rows > 14:
        print(f'Limiting dataframe to 14-days of data, today is excluded.')
        health_data_df = health_data_df.drop(health_data_df.index[15:])

    date_list = health_data_df['Date'].tolist()
    if today_date in date_list:
        if date_list.index(today_date) == 0:
            print("Found today's date in dataframe at index 0.")
        else:
            print("Did not find today's date in dataframe at index 0, aborting.")
            exit()

    violinplot_list = ['Breakfast Blood Glucose (mg/dL)', 'Lunch Blood Glucose (mg/dL)',
                       'Dinner Blood Glucose (mg/dL)', 'Bedtime Blood Glucose (mg/dL)']

    breakfast_glucose = health_data_df[violinplot_list[0]].dropna().tolist()
    lunch_glocuse = health_data_df[violinplot_list[1]].dropna().tolist()
    dinner_glocuse = health_data_df[violinplot_list[2]].dropna().tolist()
    bedtime_glocuse = health_data_df[violinplot_list[3]].dropna().tolist()

    del date_list[0]
    del breakfast_glucose[0]
    del lunch_glocuse[0]
    del dinner_glocuse[0]
    del bedtime_glocuse[0]

    # print(date_list)
    # print(breakfast_glucose)
    # print(lunch_glocuse)
    # print(dinner_glocuse)
    # print(bedtime_glocuse)

    if seven_day:
        if len(date_list) < 8:
            print('Less than 8-dates found in dataframe. 7-day graphing not applicable, '
                  'aborting.')
            exit()
        else:
            date_list = date_list[1:8]
            breakfast_glucose = breakfast_glucose[1:8]
            lunch_glocuse = lunch_glocuse[1:8]
            dinner_glocuse = dinner_glocuse[1:8]
            bedtime_glocuse = bedtime_glocuse[1:8]

    graph_figure, axis_left = matplotlib.pyplot.subplots()
    violin_plot = axis_left.violinplot([breakfast_glucose, lunch_glocuse, dinner_glocuse, bedtime_glocuse],
                                       showmeans=True)
    axis_left.set_xticks([1, 2, 3, 4], ['Breakfast', 'Lunch', 'Dinner', 'Bedtime'], rotation=90)
    axis_left.set_ylabel('Blood Glucose (mg/dL)')
    max_value = max(breakfast_glucose)
    if max(lunch_glocuse) > max_value:
        max_value = max(lunch_glocuse)
    if max(bedtime_glocuse) > max_value:
        max_value = max(bedtime_glocuse)
    max_value = round((max_value + 25) / 50) * 50
    axis_left.set_ylim([0, max_value])
    axis_left.yaxis.grid(True, 'major')

    for item in violin_plot['bodies']:
        item.set_facecolor(plot_colors[0])
        item.set_edgecolor(plot_colors[0])
        item.set_alpha(0.1)

    # Colors for each violin plot.
    black_line_color = ['black', 'black', 'black', 'black']
    red_line_color = ['red', 'red', 'red', 'red']
    violin_plot['cmaxes'].set_color(black_line_color)
    violin_plot['cmins'].set_color(black_line_color)
    violin_plot['cmeans'].set_color(red_line_color)
    violin_plot['cbars'].set_color(black_line_color)

    if violinplot_title is None:
        axis_left.set_title(f'Scott Woiwode')
    else:
        return_location = violinplot_title.find('\\n')
        if return_location > 0:
            first_line = violinplot_title[:return_location]
            second_line = violinplot_title[return_location + 2:]
            axis_left.set_title(f'Scott Woiwode{first_line}\n{second_line}')
        else:
            axis_left.set_title(f'Scott Woiwode{violinplot_title}')

    graph_figure.tight_layout()

    matplotlib.pyplot.savefig(f'glocuse_violin.png', format='png', dpi=300, bbox_inches='tight')
    if verbose:
        print(f'glocuse_violin.png write complete')

    # ----- Graph End -----
    if show_graph:
        matplotlib.pyplot.show()

print(f'{os.path.basename(__file__)} execution: {time.perf_counter() - start_time:,.2f}-seconds')
