
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>

#include "Voronoi.h"

// Modified SVG export function to include Voronoi edges
void exportStipplesToSVG(const char *filename, StippleList *stipples, int imageWidth, int imageHeight, float dotRadius, FloatImage *image)
{
    printf("%s\t%d\n", filename, stipples->count);

    // Open the file for writing
    FILE *file = fopen(filename, "w");
    if (!file)
    {
        fprintf(stderr, "Failed to open file for writing: %s\n", filename);
        return;
    }

    // SVG header
    fprintf(file, "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"%d\" height=\"%d\">\n", imageWidth, imageHeight);

    // Check if the stipple list is empty
    if (stipples == NULL || stipples->points == NULL || stipples->count == 0)
    {
        fprintf(stderr, "No stipples to export.\n");
        fclose(file);
        return;
    }

    // Compute the Voronoi diagram with edges
    VoronoiDiagram *diagram = compute_voronoi_with_edges(image, stipples);

    if (diagram)
    {
        // Draw Voronoi edges
        fprintf(file, "  <!-- Voronoi edges -->\n");
        fprintf(file, "  <g stroke=\"red\" stroke-width=\"1\" fill=\"none\" opacity=\"0.7\">\n");

        for (int i = 0; i < diagram->edgeCount; i++)
        {
            VoronoiEdge path = diagram->edges[i];

            if (path.length < 2)
                continue; // Skip too-short paths

            fprintf(file, "    <polyline points=\"");

            for (int j = 0; j < path.length; j++)
            {
                fprintf(file, "%.2f,%.2f ", (float)path.points[j].x, (float)path.points[j].y);
            }

            fprintf(file, "\"/>\n");
        }

        fprintf(file, "  </g>\n");
    }

    // Draw stipple points
    fprintf(file, "  <!-- Stipple points -->\n");
    for (int i = 0; i < diagram->count; i++)
    {
        StipplePoint pt = diagram->cells[i].centroid;
        
        // Ensure stipple coordinates are within the image bounds
        if (pt.x < 0 || pt.x >= imageWidth || pt.y < 0 || pt.y >= imageHeight)
        {
            fprintf(stderr, "Invalid stipple point coordinates: (%.2f, %.2f)\n", pt.x, pt.y);
            continue; // Skip invalid points
        }

        // Draw a circle (stipple point) at the calculated location
        fprintf(file, "  <circle cx=\"%.2f\" cy=\"%.2f\" r=\"%.2f\" fill=\"black\"/>\n", pt.x, pt.y, 4*(diagram->cells[i].sumDensity / diagram->cells[i].pixelCount));
    }

    // Free diagram when done
    free_voronoi_diagram(diagram);

    // SVG footer
    fprintf(file, "</svg>\n");

    // Close the file after writing
    fclose(file);
}

StippleList createEmptyStippleList()
{
    StippleList list;
    list.capacity = 64;
    list.count = 0;
    list.points = malloc(sizeof(StipplePoint) * list.capacity);
    return list;
}

void addToStippleList(StippleList *list, StipplePoint pt)
{
    if (list->count >= list->capacity)
    {
        list->capacity *= 2;
        list->points = realloc(list->points, sizeof(StipplePoint) * list->capacity);
    }
    list->points[list->count++] = pt;
}

void freeStippleList(StippleList *list)
{
    free(list->points);
    list->points = NULL;
    list->count = 0;
    list->capacity = 0;
}

// === Placeholder Functions ===

StippleList initializeStippleList()
{
    StippleList list = createEmptyStippleList();
    // Dummy: Add 10 random points
    for (int i = 0; i < 10; i++)
    {
        StipplePoint pt = {rand() / (float)RAND_MAX, rand() / (float)RAND_MAX};
        addToStippleList(&list, pt);
    }
    return list;
}

StipplePoint *splitCell(VoronoiCell vc)
{
    // Split a cell into 2 nearby points
    StipplePoint *points = malloc(sizeof(StipplePoint) * 2);
    points[0].x = vc.centroid.x + 0.01f;
    points[0].y = vc.centroid.y;
    points[1].x = vc.centroid.x - 0.01f;
    points[1].y = vc.centroid.y;
    return points;
}

int getSplitCount(VoronoiCell vc)
{
    return 2; // For now, always split into 2
}

// === LBG Function ===

StippleList LBG(float Tl, float Tu, FloatImage img)
{
    StippleList stippleList = initializeStippleList();
    int changed;

    printf("Initialized stipple list with %d points.\n", stippleList.count);

    int iter = 0;
    do
    {
        changed = 0;

        printf("Computing Voronoi diagram...\n");

        VoronoiDiagram *vd = compute_voronoi_with_edges(&img, &stippleList);

        printf("Voronoi diagram computed: %d cells.\n", vd->count);

        StippleList newStippleList = createEmptyStippleList();

        for (int i = 0; i < vd->count; i++)
        {
            VoronoiCell vc = vd->cells[i];
            float density = vc.sumDensity;

            if (i % 10 == 0)
                printf("Processing Voronoi cell %d/%d, density: %.4f\n", i, vd->count, density);

            if (density < Tl)
            {
                // remove point
                changed++;
            }
            else if (density > Tu)
            {
                // split point
                StipplePoint *splitPoints = splitCell(vc);
                int splitCount = getSplitCount(vc);

                for (int j = 0; j < splitCount; j++)
                {
                    addToStippleList(&newStippleList, splitPoints[j]);
                }

                free(splitPoints);
                changed++;
            }
            else
            {
                // keep point
                addToStippleList(&newStippleList, vc.centroid);
            }
        }

        freeStippleList(&stippleList);
        stippleList = newStippleList;

        printf("End of iteration, stipple count: %d\n", stippleList.count);

        char filename[100];
        snprintf(filename, sizeof(filename), "output/iteration_%d.svg", iter++ + 1);
        exportStipplesToSVG(filename, &stippleList, img.width, img.height, 2.0f, &img);

    } while (changed > 0);

    return stippleList;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <image_path>\n", argv[0]);
        return 1;
    }

    // Load image from path provided as command-line argument
    FloatImage image = loadImage(argv[1]);
    if (!image.data)
    {
        fprintf(stderr, "Failed to load image.\n");
        return 1;
    }

    float Tl = 300.0f; // Lower threshold (tweak as needed)
    float Tu = 500.0f; // Upper threshold (tweak as needed)

    // Run LBG stippling
    StippleList finalStipples = LBG(Tl, Tu, image);

    // Output results
    printf("Generated %d stipples:\n", finalStipples.count);
    for (int i = 0; i < finalStipples.count; i++)
    {
        StipplePoint pt = finalStipples.points[i];
        printf("  %.4f, %.4f\n", pt.x, pt.y); // Normalized coords (0–1)
    }

    exportStipplesToSVG("output/output_stipples.svg", &finalStipples, image.width, image.height, 2.0f, &image);

    // Clean up
    freeStippleList(&finalStipples);
    free(image.data);

    return 0;
}
