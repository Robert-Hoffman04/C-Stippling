#include <stdlib.h>
#include <math.h>
#include <float.h> // For FLT_MAX

#include "Image.h"

typedef struct
{
    float x, y;
} StipplePoint;

typedef struct
{
    StipplePoint *points;
    int count;
    int capacity;
} StippleList;

typedef struct
{
    int pixelCount;
    float sumDensity;
    StipplePoint centroid;
    // Optionally include geometry info
} VoronoiCell;
typedef struct
{
    StipplePoint *points;
    int length;
} VoronoiEdge;

typedef struct
{
    VoronoiCell *cells;
    int count;
    VoronoiEdge *edges; // Array of edges
    int edgeCount;      // Number of edges
} VoronoiDiagram;

typedef struct
{
    double xmin, xmax, ymin, ymax;
} BoundingBox;

// Euclidean distance function
float euclidean_distance(StipplePoint p1, StipplePoint p2)
{
    return sqrtf((p1.x - p2.x) * (p1.x - p2.x) + (p1.y - p2.y) * (p1.y - p2.y));
}

// Compute weighted distance where weight is based on pixel intensity
float weighted_distance(FloatImage *image, StipplePoint p1, StipplePoint p2)
{
    // Assume p1 is the pixel position and p2 is the stipple point
    int x = (int)p1.x;
    int y = (int)p1.y;
    float intensity = image->data[y * image->width + x]; // Pixel value from the image (0.0 - 1.0)

    // Inverse weight: Darker pixels (lower intensity) should have higher weight
    // Darker means higher weight: Use (1.0 - intensity) to increase weight in darker regions
    return euclidean_distance(p1, p2) * intensity; // Use intensity directly
}

#define EPSILON 1e-2
bool is_voronoi_vertex(int x, int y, FloatImage *image, StippleList *stipples)
{
    if (!stipples || stipples->count < 3)
        return false;

    float d1 = FLT_MAX, d2 = FLT_MAX, d3 = FLT_MAX;

    StipplePoint ogpt = {x, y};

    for (int i = 0; i < stipples->count; ++i)
    {
        StipplePoint pt = stipples->points[i];
        float dist_sq = weighted_distance(image, ogpt, pt);

        if (dist_sq < d1)
        {
            d3 = d2;
            d2 = d1;
            d1 = dist_sq;
        }
        else if (dist_sq < d2)
        {
            d3 = d2;
            d2 = dist_sq;
        }
        else if (dist_sq < d3)
        {
            d3 = dist_sq;
        }
    }

    // Check if top 3 distances are nearly equal
    return fabsf(d1 - d2) < EPSILON && fabsf(d2 - d3) < EPSILON;
}

// Function to determine if a pixel represents an edge between Voronoi regions
bool is_voronoi_edge(int x, int y, FloatImage *image, StippleList *stipples)
{
    // Define direction arrays for checking neighboring pixels
    const int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    const int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};

    // Find the closest stipple to the current pixel
    float min_distance = FLT_MAX;
    int closest_stipple = -1;
    StipplePoint pixel_point = {x, y};

    for (int i = 0; i < stipples->count; i++)
    {
        float dist = weighted_distance(image, pixel_point, stipples->points[i]);
        if (dist < min_distance)
        {
            min_distance = dist;
            closest_stipple = i;
        }
    }

    // Check neighboring pixels
    for (int d = 0; d < 8; d++)
    {
        int nx = x + dx[d];
        int ny = y + dy[d];

        // Skip if outside image boundaries
        if (nx < 0 || nx >= image->width || ny < 0 || ny >= image->height)
        {
            continue;
        }

        // Find closest stipple to this neighbor
        min_distance = FLT_MAX;
        int neighbor_closest_stipple = -1;
        StipplePoint neighbor_point = {nx, ny};

        for (int i = 0; i < stipples->count; i++)
        {
            float dist = weighted_distance(image, neighbor_point, stipples->points[i]);
            if (dist < min_distance)
            {
                min_distance = dist;
                neighbor_closest_stipple = i;
            }
        }

        // If the neighbor belongs to a different region, this is an edge
        if (neighbor_closest_stipple != closest_stipple)
        {
            return true;
        }
    }

    return false;
}

// Function to trace a continuous edge path starting from a given pixel
// Returns the number of segments added

int trace_edge_path(
    bool **edge_map,
    int x, int y,
    int width, int height,
    VoronoiEdge *paths, int *path_count,
    int max_paths, int max_path_length
)
{
    const int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    const int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};

    if (*path_count >= max_paths)
        return 0;

    StipplePoint *path = malloc(sizeof(StipplePoint) * max_path_length);
    if (!path)
        return 0;

    int length = 0;
    int current_x = x;
    int current_y = y;

    edge_map[current_y][current_x] = false;
    path[length++] = (StipplePoint){x, y};

    int last_dx = 0;
    int last_dy = 0;

    while (length < max_path_length)
    {
        int best_score = -1;
        int best_dir = -1;

        for (int d = 0; d < 8; d++)
        {
            int nx = current_x + dx[d];
            int ny = current_y + dy[d];

            if (nx < 0 || nx >= width || ny < 0 || ny >= height)
                continue;

            if (!edge_map[ny][nx])
                continue;

            // Direction scoring: prefer directions matching last move
            int score = 0;
            if (length > 1)
                score = dx[d] * last_dx + dy[d] * last_dy; // dot product

            if (score > best_score)
            {
                best_score = score;
                best_dir = d;
            }
        }

        if (best_dir == -1)
            break;

        current_x += dx[best_dir];
        current_y += dy[best_dir];
        edge_map[current_y][current_x] = false;

        last_dx = dx[best_dir];
        last_dy = dy[best_dir];

        path[length++] = (StipplePoint){current_x, current_y};
    }

    // Save the full path if it has enough points
    if (length >= 5)
    {
        paths[*path_count].points = path;
        paths[*path_count].length = length;
        (*path_count)++;
        return 1;
    }

    free(path);
    return 0;
}

// Modified function to compute Voronoi diagram with optimized edges
VoronoiDiagram *compute_voronoi_with_edges(FloatImage *image, StippleList *stipples)
{
    // Allocate memory for Voronoi diagram
    VoronoiDiagram *diagram = (VoronoiDiagram *)malloc(sizeof(VoronoiDiagram));
    if (!diagram)
    {
        fprintf(stderr, "Failed to allocate memory for Voronoi diagram\n");
        return NULL;
    }

    // Initialize cells
    diagram->cells = (VoronoiCell *)malloc(stipples->count * sizeof(VoronoiCell));
    diagram->count = stipples->count;
    diagram->edges = NULL; // We'll allocate this later
    diagram->edgeCount = 0;

    // Initialize all cells
    for (int i = 0; i < stipples->count; i++)
    {
        diagram->cells[i].sumDensity = 0.0f;
        diagram->cells[i].centroid.x = 0.0f;
        diagram->cells[i].centroid.y = 0.0f;
    }

    // First pass: Compute Voronoi regions and centroids
    for (int y = 0; y < image->height; y++)
    {
        for (int x = 0; x < image->width; x++)
        {
            // Find the closest stipple point to this pixel
            float min_distance = FLT_MAX;
            int closest_stipple = -1;
            StipplePoint pixel_point = {x, y};

            for (int i = 0; i < stipples->count; i++)
            {
                float dist = weighted_distance(image, pixel_point, stipples->points[i]);
                if (dist < min_distance)
                {
                    min_distance = dist;
                    closest_stipple = i;
                }
            }

            // Assign pixel to the closest stipple point's cell
            VoronoiCell *cell = &diagram->cells[closest_stipple];
            cell->sumDensity += image->data[y * image->width + x];
            cell->pixelCount++;
            cell->centroid.x += x * image->data[y * image->width + x];
            cell->centroid.y += y * image->data[y * image->width + x];
        }
    }

    // Normalize centroid positions
    for (int i = 0; i < stipples->count; i++)
    {
        if (diagram->cells[i].sumDensity > 0)
        {
            diagram->cells[i].centroid.x /= diagram->cells[i].sumDensity;
            diagram->cells[i].centroid.y /= diagram->cells[i].sumDensity;
        }
    }

    // Second pass: Identify edge pixels
    bool **edge_map = (bool **)malloc(image->height * sizeof(bool *));
    for (int y = 0; y < image->height; y++)
    {
        edge_map[y] = (bool *)calloc(image->width, sizeof(bool));
    }

    int edge_pixel_count = 0;

    for (int y = 0; y < image->height; y++)
    {
        for (int x = 0; x < image->width; x++)
        {
            if (is_voronoi_edge(x, y, image, stipples))
            {
                edge_map[y][x] = true;
                edge_pixel_count++;
            }
        }
    }

    // Allocate memory for edges - we'll use fewer edges than pixels
    int max_edges = edge_pixel_count / 5; // A more conservative estimate
    if (max_edges < 100)
        max_edges = 100; // Ensure minimum capacity

    diagram->edges = (VoronoiEdge *)malloc(max_edges * sizeof(VoronoiEdge));
    if (!diagram->edges)
    {
        fprintf(stderr, "Failed to allocate memory for Voronoi edges\n");
        for (int y = 0; y < image->height; y++)
        {
            free(edge_map[y]);
        }
        free(edge_map);
        free(diagram->cells);
        free(diagram);
        return NULL;
    }

    // Trace edge paths to create longer line segments
    for (int y = 0; y < image->height; y++)
    {
        for (int x = 0; x < image->width; x++)
        {
            if (edge_map[y][x])
            {
                trace_edge_path(edge_map, x, y,
                    image->width, image->height,
                    diagram->edges, &diagram->edgeCount,
                    100000, 100000);
            }
        }
    }

    // Clean up edge map
    for (int y = 0; y < image->height; y++)
    {
        free(edge_map[y]);
    }
    free(edge_map);

    return diagram;
}

// Function to free allocated memory for Voronoi diagram
void free_voronoi_diagram(VoronoiDiagram *diagram)
{
    if (diagram)
    {
        if (diagram->cells)
        {
            free(diagram->cells);
        }
        if (diagram->edges)
        {
            free(diagram->edges);
        }
        free(diagram);
    }
}
