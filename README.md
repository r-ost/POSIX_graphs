# POSIX_graphs

App to create directed graph structures using processes as vertices and unnamed pipes as edges.
Operations on graphs are managed by supervisor process, which can access pipes assigned to vertices.
Communication between supervisor process and user takes place by writing to FIFO file described by program argument.

Possible commands:
* print - prints all connections between vertices
* add x y - add edge between vertex x and y
* conn x y - prints info about connection between vertex x and y (there is connection, when the path between vertices exists)
