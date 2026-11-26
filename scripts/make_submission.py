"""Phase 9 - builds the submission package next to the project folder:

    Submission_Group18/
        README.txt
        1_Report/       PDC_Project_Report_Group18.docx (+ .pdf)
        2_Source_Code/  Group18_Source_Code.zip   (clean copy of the git repository)
        3_Dataset/      Group18_Dataset_ChestXray14_10000.zip  (the exact 10,000 images used)

Run:  D:\\Python\\python.exe scripts\\make_submission.py
"""
import hashlib
import shutil
import subprocess
import zipfile
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DATASET = Path("E:/X-Ray Dataset/images")
COUNT = 10_000
OUT = ROOT.parent / "Submission_Group18"
CHECKSUM = "f3eee2e41bf6a7ce"


def sha256(path, chunk=1 << 20):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while block := f.read(chunk):
            h.update(block)
    return h.hexdigest()


def main():
    if OUT.exists():
        shutil.rmtree(OUT)
    (OUT / "1_Report").mkdir(parents=True)
    (OUT / "2_Source_Code").mkdir()
    (OUT / "3_Dataset").mkdir()

    # ---- 1. Report ----------------------------------------------------------------------
    # Taken from the last commit (not the working copy), so the package matches the repository exactly.
    for ext in (".docx", ".pdf"):
        name = f"PDC_Project_Report_Group18{ext}"
        data = subprocess.run(["git", "show", f"HEAD:report/{name}"], cwd=ROOT, check=True, capture_output=True).stdout
        (OUT / "1_Report" / name).write_bytes(data)
    print("report copied (committed version)")

    # ---- 2. Source code: a clean export of the committed repository -----------------------
    # (no build outputs, no venv; the report files are already in 1_Report, viva notes are internal)
    src_zip = OUT / "2_Source_Code" / "Group18_Source_Code.zip"
    subprocess.run(["git", "archive", "--format=zip", "--prefix=PDC_Project_Group18/", "-o", str(src_zip), "HEAD",
                    "--", ".", ":(exclude)viva", ":(exclude)report/PDC_Project_Report_Group18.docx",
                    ":(exclude)report/PDC_Project_Report_Group18.pdf"], cwd=ROOT, check=True)
    print(f"source zip: {src_zip.stat().st_size / 1e6:.1f} MB")

    # ---- 3. Dataset: exactly the images the programs use (first 10,000 in name order) ------
    files = sorted(p for p in DATASET.iterdir() if p.suffix.lower() == ".png")[:COUNT]
    assert len(files) == COUNT, f"expected {COUNT} images, found {len(files)}"
    manifest = [f"{sha256(p)}  images/{p.name}" for p in files]
    readme = f"""NIH ChestX-ray14 - subset used by PDC Group 18 (Project #6)
=============================================================

Contents
  images/            {COUNT:,} chest X-rays, 1024 x 1024 PNG, 8-bit grayscale
                     ({files[0].name} ... {files[-1].name}, 2,587 patients)
  SHA256SUMS.txt     SHA-256 of every image (integrity check)

Source
  NIH Clinical Center, ChestX-ray14 (originally "ChestX-ray8"), archives images_001 and images_002.
  Official page: https://nihcc.app.box.com/v/ChestXray-NIHCC
  The disease labels are not used by this project (image processing only).

Citation
  X. Wang, Y. Peng, L. Lu, Z. Lu, M. Bagheri and R. M. Summers, "ChestX-ray8: Hospital-scale
  Chest X-ray Database and Benchmarks on Weakly-Supervised Classification and Localization of
  Common Thorax Diseases," IEEE CVPR 2017, pp. 2097-2106.

How the programs use it
  Extract the archive and pass the folder:  --input <path>/images  --count 10000
  (the default path in src/common/config.hpp is E:/X-Ray Dataset/images)
"""
    ds_zip = OUT / "3_Dataset" / "Group18_Dataset_ChestXray14_10000.zip"
    # PNG is already compressed, so the images are stored (no recompression): fast and the same size.
    with zipfile.ZipFile(ds_zip, "w", compression=zipfile.ZIP_STORED, allowZip64=True) as z:
        z.writestr("README.txt", readme, compress_type=zipfile.ZIP_DEFLATED)
        z.writestr("SHA256SUMS.txt", "\n".join(manifest) + "\n", compress_type=zipfile.ZIP_DEFLATED)
        for i, p in enumerate(files, 1):
            z.write(p, f"images/{p.name}")
            if i % 2000 == 0:
                print(f"  dataset zip: {i:,} / {COUNT:,}")
    with zipfile.ZipFile(ds_zip) as z:  # verify
        bad = z.testzip()
        n_images = sum(1 for n in z.namelist() if n.startswith("images/"))
    assert bad is None and n_images == COUNT, (bad, n_images)
    ds_size = ds_zip.stat().st_size
    print(f"dataset zip: {ds_size / 1e9:.2f} GB, {n_images:,} images, CRC test passed")

    # ---- Top-level README ----------------------------------------------------------------
    (OUT / "README.txt").write_text(f"""PDC (CS3006) Semester Project - Group 18 - Project #6
Hybrid Parallel Large-Scale Image Processing and Convolution Filtering
Muhammad Yousuf (23K-0690, Group Leader), Arsalan Ahmed (23K-0536)
Instructor: Ms. Simran Melwani            Packaged: {date.today():%d %B %Y}

1_Report/
    PDC_Project_Report_Group18.docx   the project report (PDF copy included)

2_Source_Code/Group18_Source_Code.zip
    C++20 source (sequential, OpenMP, MPI batch, MPI strips; hybrid = MPI programs with --threads),
    unit tests, build/run/benchmark scripts, benchmark results (results/benchmarks.csv),
    graph scripts and figures. See README.md inside for build and run instructions.
    Also on GitHub: https://github.com/Arsalan692/Parallel-Large-Scale-Image-Processing-and-Convolution-Filtering

3_Dataset/Group18_Dataset_ChestXray14_10000.zip   ({ds_size / 1e9:.2f} GB)
    The exact 10,000 NIH ChestX-ray14 images processed in all experiments, with SHA-256 sums.

Quick start (Windows, MSYS2 g++ with OpenMP, MS-MPI, OpenCV)
    powershell -ExecutionPolicy Bypass -File scripts\\build.ps1
    powershell -ExecutionPolicy Bypass -File scripts\\run.ps1 sequential --input <dataset>\\images --count 10000
    powershell -ExecutionPolicy Bypass -File scripts\\run.ps1 -Procs 4 mpi_batch --input <dataset>\\images --count 10000 --threads 2

Key results (10,000 images, mean of 2 runs, Intel i7-8565U 4 cores / 8 threads)
    sequential 195.1 s | OpenMP images mode, 8 threads 55.3 s (3.53x) | MPI batch, 8 processes 55.7 s (3.50x)
    every run produced the same output checksum: {CHECKSUM}
""", encoding="utf-8")

    print("\nPackage ready:", OUT)
    for p in sorted(OUT.rglob("*")):
        if p.is_file():
            print(f"  {p.relative_to(OUT)}  ({p.stat().st_size / 1e6:,.1f} MB)")


if __name__ == "__main__":
    main()
