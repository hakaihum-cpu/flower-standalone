#!/usr/bin/env python3
"""Offscreen VTK renderer for FLOWER procedural 3D blocking assets.

This gives the ChatGPT/container host an actual 3D render path even when Blender
is unavailable. It is intended for blocking, alpha checks and pose inspection.
Production art may still move to Blender later.
"""

import argparse
import importlib.util
import math
from pathlib import Path

import numpy as np
import vtk
from vtk.util import numpy_support
from PIL import Image


def load_generator(path):
    spec = importlib.util.spec_from_file_location("flower3d_blocking", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def vtk_actor(mesh):
    points = vtk.vtkPoints()
    points.SetData(
        numpy_support.numpy_to_vtk(
            np.asarray(mesh.vertices, dtype=float),
            deep=True,
        )
    )

    polygons = vtk.vtkCellArray()
    for face in np.asarray(mesh.faces, dtype=np.int64):
        triangle = vtk.vtkTriangle()
        for index in range(3):
            triangle.GetPointIds().SetId(index, int(face[index]))
        polygons.InsertNextCell(triangle)

    poly = vtk.vtkPolyData()
    poly.SetPoints(points)
    poly.SetPolys(polygons)

    colors = np.asarray(mesh.visual.face_colors, dtype=np.uint8)
    color_array = numpy_support.numpy_to_vtk(
        colors,
        deep=True,
        array_type=vtk.VTK_UNSIGNED_CHAR,
    )
    color_array.SetNumberOfComponents(4)
    color_array.SetName("Colors")
    poly.GetCellData().SetScalars(color_array)

    normals = vtk.vtkPolyDataNormals()
    normals.SetInputData(poly)
    normals.ComputePointNormalsOn()
    normals.ComputeCellNormalsOn()
    normals.SplittingOff()
    normals.ConsistencyOn()
    normals.Update()

    mapper = vtk.vtkPolyDataMapper()
    mapper.SetInputData(normals.GetOutput())
    mapper.SetScalarModeToUseCellData()
    mapper.SetColorModeToDirectScalars()
    mapper.ScalarVisibilityOn()

    actor = vtk.vtkActor()
    actor.SetMapper(mapper)
    actor.GetProperty().SetInterpolationToPhong()
    actor.GetProperty().SetAmbient(0.35)
    actor.GetProperty().SetDiffuse(0.65)
    actor.GetProperty().SetSpecular(0.04)
    return actor


def render(mesh, output, width=384, height=640):
    renderer = vtk.vtkRenderer()
    renderer.SetBackground(0.0, 0.0, 0.0)
    renderer.SetBackgroundAlpha(0)
    renderer.AddActor(vtk_actor(mesh))

    camera = renderer.GetActiveCamera()
    camera.SetPosition(0.0, -6.0, 1.05)
    camera.SetFocalPoint(0.0, 0.0, 0.82)
    camera.SetViewUp(0.0, 0.0, 1.0)
    camera.ParallelProjectionOn()
    camera.SetParallelScale(0.94)
    renderer.ResetCameraClippingRange()

    window = vtk.vtkRenderWindow()
    window.SetOffScreenRendering(1)
    window.SetAlphaBitPlanes(1)
    window.SetMultiSamples(8)
    window.AddRenderer(renderer)
    window.SetSize(width, height)
    window.Render()

    grab = vtk.vtkWindowToImageFilter()
    grab.SetInput(window)
    grab.SetInputBufferTypeToRGBA()
    grab.ReadFrontBufferOff()
    grab.Update()

    writer = vtk.vtkPNGWriter()
    writer.SetFileName(str(output))
    writer.SetInputConnection(grab.GetOutputPort())
    writer.Write()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--generator", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    generator = load_generator(args.generator)
    output = Path(args.out)
    output.mkdir(parents=True, exist_ok=True)

    for index in range(8):
        mesh = generator.build_character(
            "walk",
            2.0 * math.pi * index / 8.0,
        )
        render(
            mesh,
            output / f"student01_walk_{index + 1:02d}.png",
        )

    frames = [
        Image.open(output / f"student01_walk_{index:02d}.png").convert("RGBA")
        for index in range(1, 9)
    ]
    frames[0].save(
        output / "student01_walk_vtk.gif",
        save_all=True,
        append_images=frames[1:],
        duration=125,
        loop=0,
        disposal=2,
    )


if __name__ == "__main__":
    main()
