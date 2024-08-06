"""
Generates video based on a set of handshakes.

For initial testing, this generates synthetic data controlled by command line
params.

General datastructure will be a list of dictionaries, where each dictionary
contains a timestamp and two nodes.  It'll look something like:
    [{"timestamp": 1716153324, "nodes":[1,2]}]
"""
import random
import math
import time

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.animation as Animate

from netgraph import Graph, BaseGraph


def draw_handshakes(df, output, timeslot=60):
    """
    Draws out handshakes.
    """

    # Simulate a dynamic network with
    # - 5 frames / network states,
    # - with 10 nodes at each time point,
    # - an expected edge density of 25% at each time point
    #total_frames = 5
    #total_nodes = 10
    #adjacency_matrix = np.random.rand(total_frames, total_nodes, total_nodes) < 0.25
    #print(adjacency_matrix)

    fig, ax = plt.subplots()
    print("Building Graph")
    print(len(df))
    nodes = set(df['node1'].unique()) | set(df['node2'].unique())
    adjm = pd.DataFrame(0, columns=list(nodes), index=list(nodes))
    for index, row in df.iterrows():
        adjm.at[row['node1'], row['node2']] = 1

    print(len(nodes), list(nodes))
    g = Graph(list(zip(*[df[col] for col in df if col.startswith('node')])),
              #node_layout='circular',
              edge_width=1,
              node_size=1,
              #edge_color="w",
              arrows=False, ax=ax) # initialise with fully connected graph

    max_time = df.loc[df["timestamp"].idxmax()]['timestamp']
    min_time = df.loc[df["timestamp"].idxmin()]['timestamp']
    print(max_time, min_time, timeslot)
    frames = math.ceil((max_time-min_time)/timeslot)

    def update(ii):
        """Update graph at timeslot ii"""
        print(f"update {ii}")
        ts_min = min_time + ii*timeslot
        ts_max = min_time + (ii+1)*timeslot
        for index, row in df.iterrows():
            if ts_min <= row['timestamp'] <= ts_max:
                g.edge_artists[(row['node1'], row['node2'])].set_visible(True)
                #g.edge_artists[(row['node1'], row['node2'])].set_facecolor('b')
        #for (jj, kk), artist in g.edge_artists.items():
        #    # turn visibility of edge artists on or off, depending on the adjacency
        #    if adjacency_matrix[ii, jj, kk]:
        #        artist.set_visible(True)
        #    else:
        #        artist.set_visible(False)
        return g.edge_artists.values()

    for _, artist in g.edge_artists.items():
        artist.set_visible(False)
    print("Animating")
    anim = Animate.FuncAnimation(fig, update, frames=frames, interval=200, blit=True)
    writervideo = Animate.FFMpegWriter(fps=15)
    anim.save(output, writer=writervideo)
    plt.close()




def gen_data(timelength, nodes, connectivity):
    """
    Creates a dataframe for a given timeframe that represents handshakes between
    nodes.

    Parameters
    ----------
    timelength - time in seconds for how long the handshakes are distributed
    nodes - quantity of notes to represent
    connectivity - float, quantity of handshakes between 0 and 1

    Returns a dataframe representing adjacency list
    """
    df = pd.DataFrame(columns=['timestamp', 'node1', 'node2'])
    current_time = int(time.time())
    start_time = current_time - timelength

    # (n*(n-1))/2 for max quantity of handshakes
    handshake_count = int(((nodes*(nodes-1))/2)*connectivity)
    for i in range(handshake_count):
        node1,node2 = np.random.randint(nodes, size=2)
        while node1 == node2:
            print(f"redraw on {i}")
            node1,node2 = np.random.randint(nodes, size=2)
        df.loc[i] = [np.random.randint(start_time, current_time), node1, node2]
    return df

#
# Argparse helpers
#
def percent_float_type(arg):
    """Type function for argparse - a float between 0.0 and 1.0"""
    try:
        f = float(arg)
    except ValueError:
        raise argparse.ArgumentTypeError("Must be a floating point number")
    if f < 0.0 or f > 1.0:
        raise argparse.ArgumentTypeError("Argument must be between 0 and 1")
    return f

#
# Sort of the Entrypoint
#
def main():
    """Process arguments, run main code"""

    if args.src_file:
        # figure this interface out
        pass
    else:
        print("Generating graph")
        df = gen_data(args.gen_time_length, args.gen_nodes,
                      args.gen_connectivity)
        print("Finished graph")
    draw_handshakes(df, args.output)

#
# Real Entrypoint
#
if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description=str(__doc__),
                                     formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('-s', '--src-file',
                        help="Source file to run from, otherwise synthetic data"
                             "is used")
    parser.add_argument('--gen-time-length',
                        default=28800, # 8 hours in seconds
                        type=int,
                        help="If using synthetic data, how long is the day?")
    parser.add_argument('--gen-nodes',
                        default=100,
                        type=int,
                        help="How many nodes are participating?")
    parser.add_argument('--gen-connectivity',
                        type=percent_float_type,
                        default=.3233, # repeating of course
                        help="How connected are the nodes?")
    parser.add_argument('-o', '--output',
                        default='plt.mp4',
                        help="output_file")
    args = parser.parse_args()
    main()

