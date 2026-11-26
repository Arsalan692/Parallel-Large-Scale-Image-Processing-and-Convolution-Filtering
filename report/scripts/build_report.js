// build_report.js - generates report/PDC_Project_Report_Group18.docx
//
//   cd report/scripts && npm install && node build_report.js
//
// Figures come from analysis/make_graphs.py, analysis/make_sample_figure.py,
// report/scripts/make_diagram.py and report/scripts/make_code_figures.py.
// Benchmark numbers in the results table are read from results/summary.csv.
const fs = require("fs");
const path = require("path");
const {
  Document, Packer, Paragraph, TextRun, ImageRun, Table, TableRow, TableCell, Header, Footer,
  AlignmentType, HeadingLevel, BorderStyle, WidthType, ShadingType, VerticalAlign, PageNumber,
  TableOfContents, LevelFormat, TableLayoutType,
} = require("docx");

const ROOT = path.resolve(__dirname, "..", "..");
const FIG = path.join(ROOT, "report", "figures");
const OUT = path.join(ROOT, "report", "PDC_Project_Report_Group18.docx");

// ---- Design tokens ------------------------------------------------------------------------
const BODY = "Cambria", HEAD = "Calibri", MONO = "Consolas";
const NAVY = "1F3A5F", ACCENT = "257DE0", INK = "1F2328", GRAY = "5F6B7A", RULE = "D0D7DE", TINT = "EEF3F9";
const TEXT_W = 9026; // A4 width minus 2 x 1" margins, in DXA
const TEXT_W_PX = 602; // the same width in pixels at 96 dpi (for images)

// ---- Inline markup: **bold**, *italic*, `code`, _{sub}, ^{sup} -----------------------------
function runs(text, base = {}) {
  const out = [];
  const re = /(\*\*[^*]+\*\*|\*[^*]+\*|`[^`]+`|_\{[^}]+\}|\^\{[^}]+\})/g;
  let last = 0, m;
  while ((m = re.exec(text)) !== null) {
    if (m.index > last) out.push(new TextRun({ text: text.slice(last, m.index), ...base }));
    const t = m[0];
    if (t.startsWith("**")) out.push(new TextRun({ text: t.slice(2, -2), ...base, bold: true }));
    else if (t.startsWith("`")) out.push(new TextRun({ text: t.slice(1, -1), ...base, font: MONO, size: (base.size || 22) - 2, color: NAVY }));
    else if (t.startsWith("_{")) out.push(new TextRun({ text: t.slice(2, -1), ...base, subScript: true }));
    else if (t.startsWith("^{")) out.push(new TextRun({ text: t.slice(2, -1), ...base, superScript: true }));
    else out.push(new TextRun({ text: t.slice(1, -1), ...base, italics: true }));
    last = m.index + t.length;
  }
  if (last < text.length) out.push(new TextRun({ text: text.slice(last), ...base }));
  return out;
}

const P = (text, opts = {}) => new Paragraph({ children: runs(text), alignment: AlignmentType.JUSTIFIED, ...opts });
const lead = (title, text) => new Paragraph({
  alignment: AlignmentType.JUSTIFIED,
  children: [new TextRun({ text: title + "  ", bold: true, font: HEAD, color: NAVY, size: 23 }), ...runs(text)],
});
const bullet = (text, level = 0) => new Paragraph({ children: runs(text), numbering: { reference: "bullets", level }, alignment: AlignmentType.LEFT, spacing: { after: 60 } });
let listId = 0;
const numbered = (items) => { const ref = `num${++listId}`; numberingRefs.push(ref);
  return items.map((t) => new Paragraph({ children: runs(t), numbering: { reference: ref, level: 0 }, spacing: { after: 60 } })); };
const numberingRefs = [];

const H1 = (num, title) => new Paragraph({
  heading: HeadingLevel.HEADING_1, pageBreakBefore: true,
  children: [new TextRun({ text: `${num}`, color: ACCENT }), new TextRun({ text: `   ${title}` })],
});
const H2 = (num, title) => new Paragraph({ heading: HeadingLevel.HEADING_2, children: [new TextRun({ text: `${num}   ${title}` })] });
const spacer = (after = 120) => new Paragraph({ spacing: { before: 0, after }, children: [] });

function callout(text) {
  return new Paragraph({
    children: runs(text, { font: HEAD, size: 21, color: INK }),
    shading: { type: ShadingType.CLEAR, fill: TINT, color: "auto" },
    border: { left: { style: BorderStyle.SINGLE, size: 24, color: ACCENT, space: 8 } },
    indent: { left: 160, right: 160 }, spacing: { before: 120, after: 200, line: 288 },
  });
}

function equation(parts) { // parts: [text, style] with style in {"", "i", "sub", "sup"}
  return new Paragraph({
    alignment: AlignmentType.CENTER, spacing: { before: 120, after: 160 },
    children: parts.map(([t, s]) => new TextRun({ text: t, font: "Cambria Math", size: 23,
      italics: s === "i", subScript: s === "sub", superScript: s === "sup" })),
  });
}

// ---- Figures, listings and captions ---------------------------------------------------------
let figNo = 0, tabNo = 0, lstNo = 0;
function pngSize(file) { const b = fs.readFileSync(file); return [b.readUInt32BE(16), b.readUInt32BE(20)]; }
function caption(label, text) {
  return new Paragraph({
    alignment: AlignmentType.CENTER, spacing: { before: 80, after: 260 }, keepLines: true,
    children: [new TextRun({ text: `${label}.  `, bold: true, font: HEAD, size: 19, color: NAVY }),
      ...runs(text, { font: HEAD, size: 19, color: GRAY })],
  });
}
function image(file, widthPx) {
  const [w, h] = pngSize(file);
  return new Paragraph({
    alignment: AlignmentType.CENTER, keepNext: true, spacing: { before: 120, after: 0 },
    children: [new ImageRun({ type: "png", data: fs.readFileSync(file),
      transformation: { width: widthPx, height: Math.round(widthPx * h / w) } })],
  });
}
const figure = (file, text, widthFrac = 1) => [image(path.join(FIG, file), Math.round(TEXT_W_PX * widthFrac)), caption(`Figure ${++figNo}`, text)];
function listing(file, text) {
  const f = path.join(FIG, "code", file);
  const [w] = pngSize(f);
  // Code was rendered at 30 px per em; aim for ~7.8 pt code, but never wider than the text block.
  const natural = Math.round(w * (7.8 / 30) / 72 * 96);
  return [image(f, Math.min(TEXT_W_PX, natural)), caption(`Listing ${++lstNo}`, text)];
}

// ---- Tables ---------------------------------------------------------------------------------
const thin = { style: BorderStyle.SINGLE, size: 4, color: RULE };
const none = { style: BorderStyle.NONE, size: 0, color: "FFFFFF" };
function cell(text, width, { header = false, align = AlignmentType.LEFT, fill, bold = false, size = 19, color, pad = 55, keep = false } = {}) {
  return new TableCell({
    width: { size: width, type: WidthType.DXA },
    margins: { top: pad, bottom: pad, left: 100, right: 100 },
    verticalAlign: VerticalAlign.CENTER,
    shading: fill ? { type: ShadingType.CLEAR, fill, color: "auto" } : undefined,
    borders: { top: none, left: none, right: none, bottom: header ? none : thin },
    children: [new Paragraph({ alignment: align, keepNext: keep, spacing: { before: 0, after: 0, line: 252 },
      children: runs(String(text), { font: HEAD, size, bold: header || bold, color: header ? "FFFFFF" : (color || INK) }) })],
  });
}
// rows: arrays of strings, or {group: "title"} for a full-width group label, or {cells, bold}
function table(widths, header, rows, aligns = [], { pad = 55 } = {}) {
  const keep = rows.length <= 12; // keep small tables on one page
  const total = widths.reduce((a, b) => a + b, 0);
  const al = (i) => aligns[i] || AlignmentType.LEFT;
  const trs = [new TableRow({ tableHeader: true, children: header.map((h, i) => cell(h, widths[i], { header: true, fill: NAVY, align: al(i), pad, keep })) })];
  for (const r of rows) {
    if (r.group) {
      trs.push(new TableRow({ children: [new TableCell({
        columnSpan: widths.length, width: { size: total, type: WidthType.DXA },
        margins: { top: Math.min(50, pad), bottom: Math.min(40, pad), left: 100, right: 100 },
        shading: { type: ShadingType.CLEAR, fill: TINT, color: "auto" },
        borders: { top: none, left: none, right: none, bottom: thin },
        children: [new Paragraph({ spacing: { before: 0, after: 0 }, children: runs(r.group, { font: HEAD, size: 18, bold: true, color: NAVY }) })],
      })] }));
      continue;
    }
    const cells = r.cells || r;
    trs.push(new TableRow({ cantSplit: true, children: cells.map((c, i) => cell(c, widths[i], { align: al(i), bold: !!r.bold, pad, keep })) }));
  }
  return new Table({
    width: { size: total, type: WidthType.DXA }, columnWidths: widths, layout: TableLayoutType.FIXED,
    borders: { top: { style: BorderStyle.SINGLE, size: 8, color: NAVY }, bottom: { style: BorderStyle.SINGLE, size: 8, color: NAVY },
      left: none, right: none, insideHorizontal: thin, insideVertical: none },
    rows: trs,
  });
}
const tableCaption = (text) => new Paragraph({
  keepNext: true, spacing: { before: 200, after: 100 },
  children: [new TextRun({ text: `Table ${++tabNo}.  `, bold: true, font: HEAD, size: 19, color: NAVY }), ...runs(text, { font: HEAD, size: 19, color: GRAY })],
});
const C = AlignmentType.CENTER, R = AlignmentType.RIGHT;

// ---- Benchmark data -------------------------------------------------------------------------
const csv = fs.readFileSync(path.join(ROOT, "results", "summary.csv"), "utf8").trim().split(/\r?\n/);
const cols = csv[0].split(",");
const S = {};
for (const line of csv.slice(1)) { const v = line.split(","); const o = {}; cols.forEach((c, i) => (o[c] = v[i])); S[o.id] = o; }
const f1 = (x) => Number(x).toFixed(1);
function resultRow(id, label, bold = false) {
  const s = S[id];
  return { bold, cells: [label, `${s.procs} × ${s.threads}`, f1(s.mean_s), `${Number(s.speedup).toFixed(2)}×`,
    `${Math.round(Number(s.efficiency_pct))}%`, f1(s.read_s), f1(s.compute_s), f1(Math.max(0, s.comm_s))] };
}
const T_SEQ = f1(S.seq.mean_s);

// =============================================================================================
// COVER
// =============================================================================================
const stat = (value, label) => new TableCell({
  width: { size: TEXT_W / 4, type: WidthType.DXA },
  margins: { top: 80, bottom: 80, left: 160, right: 80 },
  borders: { top: none, bottom: none, right: none, left: { style: BorderStyle.SINGLE, size: 18, color: ACCENT } },
  children: [
    new Paragraph({ spacing: { after: 0 }, children: [new TextRun({ text: value, font: HEAD, size: 40, bold: true, color: NAVY })] }),
    new Paragraph({ spacing: { after: 0 }, children: [new TextRun({ text: label, font: HEAD, size: 17, color: GRAY })] }),
  ],
});
const cover = [
  new Paragraph({ spacing: { before: 600, after: 40 }, children: [new TextRun({ text: "NATIONAL UNIVERSITY OF COMPUTER AND EMERGING SCIENCES", font: HEAD, size: 19, bold: true, color: NAVY, characterSpacing: 20 })] }),
  new Paragraph({ spacing: { after: 0 }, children: [new TextRun({ text: "FAST School of Computing  ·  Karachi Campus", font: HEAD, size: 19, color: GRAY })] }),
  new Paragraph({ spacing: { before: 2300, after: 160 }, children: [new TextRun({ text: "CS3006  ·  PARALLEL AND DISTRIBUTED COMPUTING  ·  SEMESTER PROJECT", font: HEAD, size: 19, bold: true, color: ACCENT, characterSpacing: 20 })] }),
  new Paragraph({ spacing: { after: 200, line: 240 }, children: [new TextRun({ text: "Hybrid Parallel Large-Scale Image Processing and Convolution Filtering", font: HEAD, size: 56, bold: true, color: NAVY })] }),
  new Paragraph({ spacing: { after: 320 }, border: { bottom: { style: BorderStyle.SINGLE, size: 12, color: ACCENT, space: 14 } },
    children: [new TextRun({ text: "Gaussian blur, sharpening and Sobel edge detection on 10,000 chest X-rays with OpenMP, MPI and hybrid MPI + OpenMP", font: BODY, size: 26, italics: true, color: GRAY })] }),
  new Table({ width: { size: TEXT_W, type: WidthType.DXA }, columnWidths: [TEXT_W / 4, TEXT_W / 4, TEXT_W / 4, TEXT_W / 4],
    layout: TableLayoutType.FIXED, borders: { top: none, bottom: none, left: none, right: none, insideHorizontal: none, insideVertical: none },
    rows: [new TableRow({ children: [stat("10,000", "chest X-rays processed"), stat("3", "hand-written filters"),
      stat("3.5×", "speedup on 4 cores"), stat("54 / 54", "runs bit-identical")] })] }),
  new Paragraph({ spacing: { before: 2600, after: 120 }, children: [new TextRun({ text: "SUBMITTED BY", font: HEAD, size: 17, bold: true, color: GRAY, characterSpacing: 20 })] }),
  table([4600, 2200], ["Group member", "Roll number"], [["Muhammad Yousuf (Group Leader)", "23K-0690"], ["Arsalan Ahmed", "23K-0536"]]),
  new Paragraph({ spacing: { before: 300, after: 40 }, children: [
    new TextRun({ text: "Submitted to  ", font: HEAD, size: 20, color: GRAY }), new TextRun({ text: "Ms. Simran Melwani", font: HEAD, size: 20, bold: true, color: INK })] }),
  new Paragraph({ spacing: { after: 40 }, children: [new TextRun({ text: "Group 18  ·  Project #6  ·  BCS-7C", font: HEAD, size: 20, color: GRAY })] }),
  new Paragraph({ spacing: { after: 0 }, children: [new TextRun({ text: "November 2026", font: HEAD, size: 20, color: GRAY })] }),
];

// =============================================================================================
// ABSTRACT + CONTENTS
// =============================================================================================
const frontTitle = (t) => new Paragraph({ pageBreakBefore: true, spacing: { before: 0, after: 240 },
  children: [new TextRun({ text: t, font: HEAD, size: 40, bold: true, color: NAVY })] });

const abstract = [
  frontTitle("Abstract"),
  P("Convolution is among the most common operations in image processing, and among the most naturally parallel: every output pixel depends only on a small neighbourhood of the input. This project applies three hand-written convolution filters (a 5 × 5 Gaussian blur, a 3 × 3 sharpening kernel and Sobel edge detection) to 10,000 chest radiographs from the NIH ChestX-ray14 dataset, a workload of 10.5 gigapixels stored in 4.0 GB of PNG files. The same convolution routine is parallelised with OpenMP, with MPI and with a hybrid of the two, using two decompositions: distributing whole images across workers, and splitting every image into horizontal strips that exchange halo rows with their neighbours."),
  P(`On a 4-core, 8-thread laptop the sequential pipeline takes ${T_SEQ} s. Whole-image distribution finishes in 55–57 s with every programming model, a speedup of 3.5×, whereas strip decomposition peaks at 1.53× because image decoding stays serial on one process and every image costs seven collective and point-to-point operations. Across all 54 benchmark runs (27 configurations, each repeated twice) the output is bit-identical to the sequential version, verified with a 64-bit checksum over all 30,000 output images. The measurements make three classic effects concrete: Amdahl’s law, the limited value of hyper-threads, and the communication overhead that makes hybrid MPI + OpenMP outperform pure MPI by 25% for strip decomposition.`),
  new Paragraph({ spacing: { before: 200 }, children: [new TextRun({ text: "Keywords  ", bold: true, font: HEAD, color: NAVY, size: 21 }),
    new TextRun({ text: "parallel computing, MPI, OpenMP, hybrid parallelism, image convolution, domain decomposition, halo exchange, speedup, Amdahl’s law", italics: true, size: 21, color: GRAY })] }),
];

const contents = [
  frontTitle("Contents"),
  new TableOfContents("Contents", { hyperlink: true, headingStyleRange: "1-2" }),
];

// =============================================================================================
// 1  INTRODUCTION
// =============================================================================================
const intro = [
  H1(1, "Introduction"),
  H2("1.1", "Background"),
  P("A convolution filter slides a small grid of weights, the *kernel*, across an image and replaces every pixel with a weighted sum of its neighbours. Different kernels produce very different effects: a Gaussian kernel smooths noise, a sharpening kernel exaggerates local contrast, and a pair of Sobel kernels highlights edges. In medical imaging these operations are routine pre-processing steps, used to suppress acquisition noise, enhance fine structures and extract contours before an image is read by a radiologist or a diagnostic model [1, 5]."),
  P("Filtering one 1024 × 1024 image takes about ten milliseconds, but medical archives and training sets contain tens of thousands of images, which turns a trivial operation into minutes of computation. Because every output pixel can be computed independently, the work can be split at the level of pixels, rows or whole images. This makes convolution an ideal problem for studying how decomposition strategies and programming models behave on real hardware."),
  H2("1.2", "Problem statement"),
  callout("Given 10,000 grayscale chest radiographs, compute a blurred, a sharpened and an edge-detected version of every image as fast as possible on a multi-core machine, while producing output that is identical, bit for bit, to a simple sequential implementation."),
  H2("1.3", "Objectives"),
  ...numbered([
    "Implement the three filters by hand in C++, using OpenCV only to read and write image files, together with a sequential baseline.",
    "Parallelise the workload with OpenMP (shared memory), MPI (message passing) and a hybrid of MPI processes running OpenMP threads.",
    "Compare coarse-grained decomposition (whole images per worker) with fine-grained decomposition (rows or strips of each image), including the halo exchange required at strip boundaries.",
    "Measure execution time, speedup and efficiency for different numbers of processes and threads, and explain the results.",
  ]),
];

// =============================================================================================
// 2  DATASET
// =============================================================================================
const dataset = [
  H1(2, "Dataset Description"),
  P("The project uses **NIH ChestX-ray14**, a public collection released by the NIH Clinical Center [1]. The full dataset contains 112,120 frontal chest radiographs from 30,805 patients, distributed as twelve archives. We downloaded the first two archives (14,999 images), verified them against the published SHA-256 hashes, and process the first 10,000 images in file-name order. The disease labels that accompany the dataset are not used: the task here is image processing, not classification. Figure 1 shows a sample image with the three outputs produced by our implementation."),
  tableCaption("The workload at a glance."),
  table([3000, 6026], ["Property", "Value"], [
    ["Source", "NIH Clinical Center, ChestX-ray14 (Wang et al., CVPR 2017)"],
    ["Images processed", "10,000 (from 2,587 patients); 14,999 available locally"],
    ["Resolution and format", "1024 × 1024 pixels, PNG, 8-bit grayscale (about 1% stored as RGBA and converted on load)"],
    ["Size on disk", "3.98 GB for the 10,000 images"],
    ["Pixels per filter", "10.49 × 10^{9} (10.5 gigapixels)"],
    ["Arithmetic", "52 multiply–add operations per pixel, about 5.5 × 10^{11} in total"],
    ["Output", "30,000 filtered images (blur, sharpen and edges for every input)"],
  ]),
  ...figure("sample_filters.png", "Image 00000001_000.png and its three filtered versions. The bottom row enlarges the boxed region: the blur suppresses noise, the sharpened image accentuates fine texture, and the Sobel filter keeps only edges such as the heart border and ribs.", 0.86),
  spacer(80),
  P("The dataset suits a parallel-computing study for three reasons. It is **large enough to matter**: the sequential version needs more than three minutes, so speedups are clearly measurable. It is **uniform**: every image has the same size, so differences between runs reflect the parallel strategy rather than an uneven workload. And it is **realistic**: smoothing, sharpening and edge extraction are exactly the operations applied to radiographs in practice."),
];

// =============================================================================================
// 3  METHODOLOGY
// =============================================================================================
const method = [
  H1(3, "Methodology"),
  H2("3.1", "Convolution filters"),
  P("For a kernel *K* of size *k* × *k* with radius *r* = ⌊*k*/2⌋ and divisor *d*, the output pixel at column *x* and row *y* is"),
  equation([["O", "i"], ["(", ""], ["x", "i"], [", ", ""], ["y", "i"], [")  =  ", ""], ["1", ""], [" / ", ""], ["d", "i"], ["  ·  Σ", ""], ["i,j", "sub"], ["  ", ""], ["K", "i"], ["(", ""], ["i", "i"], [", ", ""], ["j", "i"], [")  ·  ", ""], ["I", "i"], ["(", ""], ["x", "i"], [" + ", ""], ["j", "i"], [" − ", ""], ["r", "i"], [",  ", ""], ["y", "i"], [" + ", ""], ["i", "i"], [" − ", ""], ["r", "i"], [")", ""]]),
  P("Pixels that fall outside the image are replaced by the nearest edge pixel (replicate border). All kernels use integer weights, so the arithmetic is exact and the result does not depend on the order in which pixels are processed. This property is what makes bit-identical output across all versions possible. Table 2 lists the kernels."),
  tableCaption("The three filters."),
  table([1900, 4126, 900, 2100], ["Filter", "Kernel", "Size", "Output"], [
    ["Gaussian blur", "binomial (1 4 6 4 1)^{T} × (1 4 6 4 1), divisor 256", "5 × 5", "rounded weighted mean"],
    ["Sharpen", "[0 −1 0;  −1 5 −1;  0 −1 0]", "3 × 3", "clamped to 0–255"],
    ["Sobel edges", "G_{x} = [−1 0 1;  −2 0 2;  −1 0 1],  G_{y} = G_{x}^{T}", "3 × 3 (×2)", "√(G_{x}^{2} + G_{y}^{2}), clamped"],
  ], [undefined, undefined, C]),
  spacer(60),
  P("All three filters are evaluated in a single pass. For each row, the routine fetches the five source rows the 5 × 5 kernel needs once, and computes the blurred, sharpened and edge pixels together."),
  H2("3.2", "Design principles"),
  P("Four rules keep the comparison fair and the results trustworthy:"),
  bullet("**One shared kernel.** Every program calls the same `filter_row` function. The versions differ only in how the work is divided, so performance differences come from parallelisation alone."),
  bullet("**Verified correctness.** Every run computes a 64-bit FNV-1a checksum of all output images. Per-image checksums are combined by addition, which is order-independent and therefore works with OpenMP and MPI reductions. Every parallel run must reproduce the sequential checksum exactly."),
  bullet("**Separate timers.** Reading (PNG decoding), computation and communication are timed separately, so that each result can be explained, not just reported."),
  bullet("**One program for MPI and hybrid.** The MPI programs accept a thread count; pure MPI is simply the hybrid program with one thread per process. Only a small sample of outputs is written to disk, so file writing does not distort the timings."),
  H2("3.3", "Parallel strategies"),
  P("Five programs implement the strategies summarised in Table 3 and illustrated in Figure 2. The key distinction is **granularity**. Coarse-grained strategies give each worker whole images and need almost no coordination. Fine-grained strategies split each image among the workers, which requires synchronisation and, with MPI, data movement for every image."),
  tableCaption("Parallel versions and how each one divides the work."),
  table([1700, 2300, 2400, 2626], ["Version", "Unit of work", "Inside each worker", "Communication"], [
    ["Sequential", "all images, one by one", "one thread", "none"],
    ["OpenMP rows", "rows of one image", "threads share the 1,024 rows; one thread reads", "shared memory, barrier per image"],
    ["OpenMP images", "whole images", "each thread reads and filters its own images", "shared memory, reduction of checksum"],
    ["MPI batch", "block of whole images per process", "T OpenMP threads (hybrid)", "`MPI_Bcast` file list, `MPI_Reduce` checksum"],
    ["MPI strips", "horizontal strip of every image", "T OpenMP threads (hybrid)", "per image: Bcast, Scatterv, 2 × Sendrecv, 3 × Gatherv"],
  ]),
  ...figure("decomposition.png", "The two MPI decompositions. (a) Batch: the image list is split into contiguous blocks, one per process. (b) Strips: every image is split into horizontal strips; neighbouring processes exchange two halo rows so that the 5 × 5 kernel can be evaluated at strip boundaries."),
  P("**Halo exchange.** A pixel near the top or bottom of a strip needs up to two rows that belong to the neighbouring strip. Each process therefore allocates its strip plus two *halo* rows on each side and fills them with two `MPI_Sendrecv` calls: first every process sends its top rows up while receiving its bottom halo from below, then the reverse. Combining send and receive in one call avoids the deadlock that paired blocking sends could cause. Using `MPI_PROC_NULL` as the neighbour of the first and last process turns their missing exchange into a no-op, so no special cases are needed."),
  H2("3.4", "Experimental setup"),
  tableCaption("Hardware and software."),
  table([2600, 6426], ["Component", "Details"], [
    ["Processor", "Intel Core i7-8565U: 4 cores, 8 hardware threads, 1.8 GHz base, up to 4.6 GHz turbo"],
    ["Memory", "16 GB"],
    ["Operating system", "Windows 11 Pro"],
    ["Compiler", "GCC 16.2 (MSYS2 MinGW-w64), `-O3 -march=native -fopenmp`"],
    ["MPI", "Microsoft MPI 10.1"],
    ["Image I/O", "OpenCV 5.0, used only for `imread` and `imwrite`"],
  ]),
  spacer(80),
  P("The benchmark covers **27 configurations**: the sequential program; OpenMP with 1, 2, 4 and 8 threads in both modes (plus dynamic scheduling at 8 threads); MPI batch and MPI strips with 1, 2, 4 and 8 processes; and the hybrid splits 1 × 8, 2 × 4, 4 × 2 and 2 × 2 (processes × threads) for both decompositions. A script runs every configuration on all 10,000 images, **twice**, with the repeat loop outermost so that any slow drift of the machine affects every configuration equally. The laptop ran on mains power in the *Best performance* mode with other applications closed, and the dataset folder was excluded from antivirus scanning. Each reported time is the mean of the two runs. With *P* processes and *T* threads per process, speedup and efficiency are defined as"),
  equation([["S", "i"], ["  =  ", ""], ["T", "i"], ["seq", "sub"], ["  /  ", ""], ["T", "i"], ["par", "sub"], ["          ", ""], ["E", "i"], ["  =  ", ""], ["S", "i"], ["  /  (", ""], ["P", "i"], [" × ", ""], ["T", "i"], [")", ""]]),
  P(`where *T*_{seq} = ${T_SEQ} s is the sequential time measured in the same session. All MPI processes run on the same machine, so messages travel through shared memory rather than a network. This makes communication cheaper than on a cluster, which means the strip results reported here are, if anything, optimistic.`),
];

// =============================================================================================
// 4  IMPLEMENTATION
// =============================================================================================
const impl = [
  H1(4, "Implementation"),
  H2("4.1", "Code organisation"),
  P("The project is written in C++20 and builds with a single command. All programs accept the same command-line options (`--count`, `--threads`, `--schedule`, `--mode`, `--csv`), and the shared code in `src/common` is compiled once and linked into every program."),
  tableCaption("Source tree."),
  table([2300, 6726], ["Path", "Contents"], [
    ["`src/common/`", "kernels, convolution, image I/O, block partitioning, checksum, timing, options, CSV output"],
    ["`src/sequential/`", "sequential baseline"],
    ["`src/openmp/`", "OpenMP version with rows and images modes"],
    ["`src/mpi_batch/`", "MPI and hybrid version distributing whole images"],
    ["`src/mpi_strips/`", "MPI and hybrid version with strip decomposition and halo exchange"],
    ["`tests/`", "unit tests and a validation against OpenCV’s own filters"],
    ["`scripts/`, `analysis/`", "build, run and benchmark scripts; graph generation"],
  ]),
  spacer(80),
  P("The listings below are taken directly from the source files, with their original line numbers. Highlighted lines mark where a parallel-computing concept is applied."),

  H2("4.2", "Shared convolution kernel"),
  P("Listing 1 shows the core of the computation. `weighted_sum` is a template with a compile-time flag: interior pixels, which make up more than 99% of each row, take a fast path with no bounds checks, while the two border columns on each side clamp their coordinates. `filter_pixel` evaluates all four kernels for one pixel and rounds and clamps the results. Every version, sequential or parallel, calls exactly this code."),
  ...listing("listing1_convolution.png", "The shared convolution routine (`src/common/convolution.cpp`)."),

  H2("4.3", "OpenMP: rows mode and images mode"),
  P("In **rows mode** (Listing 2), the images are processed one after another and `#pragma omp parallel for` divides the 1,024 rows of each image among the threads. Every iteration reads the shared input and writes only its own output row, so no locks are needed. The implicit barrier at the end of the loop guarantees that an image is complete before the next one is read. `schedule(runtime)` lets the program choose static or dynamic scheduling from the command line through `omp_set_schedule`."),
  ...listing("listing2_omp_rows.png", "Fine-grained OpenMP: threads share the rows of each image."),
  P("In **images mode** (Listing 3), the loop runs over images instead, so each thread reads, decodes and filters whole images on its own. Three synchronisation constructs make this safe. The `reduction(+ : …)` clause gives every thread private copies of the checksum and timing accumulators and adds them together at the end, which avoids data races without locking. A `critical` section records an error message, because exceptions may not escape a parallel region. An `atomic capture` updates the shared progress counter."),
  ...listing("listing3_omp_images.png", "Coarse-grained OpenMP with reduction, critical section and atomic update."),

  H2("4.4", "MPI batch distribution and the hybrid model"),
  P("Listing 4 shows the batch program. `MPI_Init_thread` requests `MPI_THREAD_FUNNELED`: OpenMP threads run inside each process, but only the main thread calls MPI, and only outside parallel regions. This is the cheapest thread-support level that is still correct for the design. Rank 0 lists the image directory and broadcasts the list as one packed string. Each process then computes its own block of images with `split_range` (Listing 5) and filters it with the same OpenMP loop as images mode. A single `MPI_Reduce` sums the per-process checksums. With 8 processes, every rank receives exactly 1,250 images, so the load is perfectly balanced."),
  ...listing("listing4_mpi_batch.png", "MPI batch program: thread-level initialisation, broadcast, block partitioning, OpenMP within each process, and reduction."),
  ...listing("listing5_partition.png", "Block partitioning: part sizes differ by at most one item."),

  H2("4.5", "MPI strips with halo exchange"),
  P("Listing 6 shows the per-image communication of the strip program. Rank 0 reads the image and `MPI_Scatterv` sends each process its rows. Variable counts are needed because the image height need not be divisible by the number of processes. Two `MPI_Sendrecv` calls fill the halo rows, the strip is filtered by OpenMP threads, and three `MPI_Gatherv` calls reassemble the blurred, sharpened and edge images on rank 0."),
  P("The convolution routine itself is unaware of strips. It receives an `ImageView`, a window of rows described in global image coordinates, so the same `filter_row` works on a whole image or on a strip with its halo. A debug assertion catches any access outside the window, which would indicate an insufficient halo."),
  ...listing("listing6_mpi_strips.png", "Strip decomposition: scatter, halo exchange, OpenMP filtering of the strip, and gather."),

  H2("4.6", "Verification"),
  P("Correctness was established at three levels before any performance was measured:"),
  bullet("**Unit tests.** Nine tests pass. They check the kernel weights, hand-computed filter values, block partitioning and the checksum. One test simulates the strip program for 1, 2, 3, 4, 7 and 8 strips and confirms that strips with halos reproduce the whole-image result exactly."),
  bullet("**Validation against OpenCV.** On 50 X-rays, our sharpening and Sobel outputs are 100% identical to OpenCV’s `filter2D` and `Sobel`. The blur is identical for 99.82% of pixels; the remaining 0.18% are exact .5 rounding ties, which we round up and OpenCV rounds to even. There are no genuine mismatches."),
  bullet("**Checksum of every run.** All 54 benchmark runs produced the same checksum, `f3eee2e41bf6a7ce`, as the sequential program."),

  H2("4.7", "Where each PDC concept is applied"),
  tableCaption("Parallel-computing concepts and their location in the code."),
  table([2450, 3500, 3076], ["Concept", "Construct and location", "Purpose"], [
    { group: "OpenMP (shared memory)" },
    ["Work-sharing loop", "`parallel for`: openmp:49, 86; mpi_batch:101", "divide rows or images among threads"],
    ["Loop scheduling", "`schedule(runtime)` + `omp_set_schedule`", "compare static and dynamic scheduling"],
    ["Reduction", "`reduction(+ : …)`: openmp:86; mpi_batch:101", "race-free checksum and timing sums"],
    ["Mutual exclusion", "`critical`: openmp:110", "record an error from any thread"],
    ["Atomic operation", "`atomic capture`: openmp:115", "shared progress counter"],
    ["Barrier", "implicit, end of `parallel for`", "finish an image before the next"],
    { group: "MPI (message passing)" },
    ["Broadcast", "`MPI_Bcast`: mpi_batch:78, 83; mpi_strips", "file list and image size to all ranks"],
    ["Data decomposition", "`split_range`: partition.hpp:16", "balanced blocks of images or rows"],
    ["Scatter and gather", "`MPI_Scatterv` / `MPI_Gatherv`: mpi_strips:143, 175", "distribute strips, collect results"],
    ["Halo exchange", "`MPI_Sendrecv` + `MPI_PROC_NULL`: mpi_strips:148, 152", "exchange boundary rows"],
    ["Global reduction", "`MPI_Reduce`: mpi_batch:137", "combine checksums on rank 0"],
    ["Synchronised timing", "`MPI_Barrier` + `MPI_Wtime`", "common start time for all ranks"],
    { group: "Hybrid" },
    ["Thread support level", "`MPI_Init_thread(FUNNELED)`: mpi_batch:39", "OpenMP threads inside MPI ranks"],
  ]),
];

// =============================================================================================
// 5  RESULTS
// =============================================================================================
const W8 = [2560, 850, 950, 950, 1000, 880, 1000, 836];
const resultsTable = table(W8, ["Configuration", "P × T", "Time (s)", "Speedup", "Efficiency", "Read (s)", "Compute (s)", "Comm (s)"], [
  { group: "Sequential" }, resultRow("seq", "Sequential baseline"),
  { group: "OpenMP: rows mode (static schedule)" },
  resultRow("omp-rows-2", "2 threads"), resultRow("omp-rows-4", "4 threads"), resultRow("omp-rows-8", "8 threads"),
  { group: "OpenMP: images mode (static schedule)" },
  resultRow("omp-images-2", "2 threads"), resultRow("omp-images-4", "4 threads"), resultRow("omp-images-8", "8 threads", true),
  { group: "OpenMP: dynamic schedule" },
  resultRow("omp-rows-8-dynamic", "rows mode, 8 threads"), resultRow("omp-images-8-dynamic", "images mode, 8 threads"),
  { group: "MPI batch (one thread per process)" },
  resultRow("batch-2x1", "2 processes"), resultRow("batch-4x1", "4 processes"), resultRow("batch-8x1", "8 processes", true),
  { group: "MPI strips (one thread per process)" },
  resultRow("strips-2x1", "2 processes"), resultRow("strips-4x1", "4 processes"), resultRow("strips-8x1", "8 processes"),
  { group: "Hybrid batch" },
  resultRow("batch-1x8", "1 process × 8 threads"), resultRow("batch-2x4", "2 processes × 4 threads"),
  resultRow("batch-4x2", "4 processes × 2 threads"), resultRow("batch-2x2", "2 processes × 2 threads"),
  { group: "Hybrid strips" },
  resultRow("strips-1x8", "1 process × 8 threads"), resultRow("strips-2x4", "2 processes × 4 threads"),
  resultRow("strips-4x2", "4 processes × 2 threads"), resultRow("strips-2x2", "2 processes × 2 threads"),
], [undefined, C, R, R, R, R, R, R], { pad: 30 });

const results = [
  H1(5, "Results"),
  P(`All values are the mean of two runs on the full set of 10,000 images. The sequential baseline takes **${T_SEQ} s**: ${f1(S.seq.read_s)} s reading and decoding PNG files and ${f1(S.seq.compute_s)} s filtering. Table 7 lists every parallel configuration. The 1-worker runs of each program, all within 1.3% of the baseline, are omitted for brevity. Read, compute and communication times are measured on rank 0 or averaged over threads.`),
  tableCaption("Benchmark results for 10,000 images (best results in bold)."),
  resultsTable,
  callout(`**Key result.** The best configurations, OpenMP images mode and MPI batch with 8 workers, process the 10,000 X-rays in **55.3 s and 55.7 s**, against ${T_SEQ} s sequentially: a **3.5× speedup**, with output identical to the sequential program in every one of the 54 runs.`),

  H2("5.1", "Execution time and speedup"),
  P("Figures 3 and 4 show how the four strategies scale from one to eight workers. The two coarse-grained strategies, OpenMP images mode and MPI batch, are practically indistinguishable. Both halve the time with two workers (103 s), reach 67 s with four, and 55–56 s with eight. The fine-grained strategies flatten out early: OpenMP rows mode improves only from 146.8 s to 122.4 s between two and eight threads, and MPI strips is fastest at four processes (141.0 s), then becomes slower at eight (170.4 s)."),
  ...figure("fig1_time.png", "Execution time for 1–8 workers. The shaded region marks workers that run on hyper-threads rather than additional physical cores.", 0.86),
  ...figure("fig2_speedup.png", "Speedup over the sequential version, with the ideal linear speedup for reference.", 0.86),

  H2("5.2", "Efficiency"),
  P("Efficiency (Figure 5) measures how much of each added worker is actually used. The coarse-grained strategies keep 94–95% efficiency with two workers and 73% with four, then fall to 44% at eight, when the extra workers are hyper-threads sharing the same four cores. The fine-grained strategies lose efficiency much faster, falling to 62–66% with only two workers, because a growing share of each run is serial."),
  ...figure("fig3_efficiency.png", "Parallel efficiency, defined as speedup divided by the number of workers.", 0.86),

  H2("5.3", "Hybrid configurations"),
  P("Figure 6 keeps the number of workers fixed at eight and varies the split between MPI processes and OpenMP threads. For batch distribution the split makes no meaningful difference (55.7–57.1 s): the processes barely communicate, and threads and processes compete for the same four cores in the same way. For strip decomposition the split matters a great deal. One process with eight threads takes 127.3 s, while eight single-threaded processes take 170.4 s, so **fewer processes and more threads is 25% faster**."),
  ...figure("fig4_hybrid.png", "Eight workers arranged as processes × threads. Batch is insensitive to the split; strips benefits strongly from fewer processes.", 0.9),

  H2("5.4", "Where the time goes"),
  P("Figure 7 breaks each run into reading, computation, communication and other time (checksumming and waiting). It explains every result above. Computation shrinks from 103.7 s to about 27–33 s in *all* parallel versions; the difference between the fast and slow versions lies entirely in reading. Where every worker reads its own images, reading falls from 85.5 s to about 21 s. Where rank 0 or a single thread reads every image (rows mode and all strip configurations), reading stays at 85 s or more and dominates the run. In the strip version with eight processes it even grows to 113 s, and communication adds a further 12 s."),
  ...figure("fig5_breakdown.png", "Breakdown of the run time for representative configurations (values in seconds)."),

  H2("5.5", "Consistency"),
  P("The two runs of each configuration differ by less than 2% for 23 of the 27 configurations; the largest spread is 7%, for strips with two processes. Static and dynamic scheduling give the same times to within 1% (122.4 s versus 123.0 s in rows mode, 55.3 s versus 55.7 s in images mode). The repeats also corrected an earlier single-run measurement: a hybrid batch time of 50.1 s observed during development turned out to be noise; repeated, the configuration takes 57.1 s."),
];

// =============================================================================================
// 6  OBSERVATIONS & CONCLUSION
// =============================================================================================
const fSerial = (Number(S.seq.read_s) / Number(S.seq.mean_s));
const concl = [
  H1(6, "Observations & Conclusion"),
  H2("6.1", "Observations"),
  lead("1. Granularity decides the outcome.", "Giving each worker whole images was the best strategy under every programming model. The workers never wait for each other and exchange almost nothing (MPI batch spends 0.2 s on communication in a 55.7 s run), so OpenMP, MPI and every hybrid split converge to the same time. On a single shared-memory machine, how the workers are organised matters far less than what each worker is given to do."),
  lead("2. Amdahl’s law sets a hard ceiling.", `Reading and decoding the PNG files accounts for ${Math.round(fSerial * 100)}% of the sequential time. In rows mode this part remains serial, so no number of threads can push the speedup beyond 1 / ${fSerial.toFixed(2)} = ${(1 / fSerial).toFixed(2)}×. The measured speedups (1.33×, 1.53× and 1.59× for 2, 4 and 8 threads) track the Amdahl prediction closely (Figure 8). Images mode escapes the limit by parallelising the reading as well. The practical lesson is to profile first: the convolution itself, the obvious hotspot, was only ${Math.round(Number(S.seq.compute_s) / Number(S.seq.mean_s) * 100)}% of the work.`),
  ...figure("fig6_amdahl.png", `Measured speedup against Amdahl’s law with serial fraction f = ${fSerial.toFixed(2)}. Rows mode stays below the predicted curve; images mode, which also parallelises reading, is not bound by it.`, 0.86),
  lead("3. Four cores are not eight.", "The processor has four physical cores; threads five to eight are hyper-threads that share a core’s execution units. Going from four to eight workers therefore gains only about 20% (66.6 s to 55.3 s). Even at four workers, reading and computing are each about 1.35× slower per worker than ideal (28.8 s instead of 21.4 s, and 35.5 s instead of 25.9 s). This is consistent with the processor lowering its clock when all cores are busy, and with the cores sharing one L3 cache and memory bus."),
  lead("4. Communication and busy-waiting limit strip decomposition.", "Per image, the strip version scatters 1 MB, exchanges halos and gathers 3 MB: about 40 GB of data movement for the whole dataset, with a synchronisation point for every image. Worse, MPI processes that are waiting in a collective operation poll actively rather than sleep. With eight processes on four cores, seven polling processes compete with rank 0 for CPU time, and its reading slows from 85 s to 113 s. This is why eight processes are slower than four."),
  lead("5. Hybrid pays off when communication matters.", "Replacing MPI processes with OpenMP threads reduces the number of participants in every collective call, the volume of halo traffic and the number of busy-waiting processes, while the threads still share the filtering. For strips this improves the time from 170.4 s (8 × 1) to 127.3 s (1 × 8). This is the standard argument for hybrid programming on clusters: one MPI process per node for communication between nodes, and threads within the node."),
  lead("6. Load balance was never the problem.", "Every image has the same size and every row the same cost, so block partitioning and static scheduling already distribute the work evenly. Dynamic scheduling only adds overhead, which in this case is negligible. It would pay off with images of varying size."),
  lead("7. Image I/O is computation too.", "Reading time fell almost in proportion to the number of parallel readers (85.5 s, 45.4 s and 28.8 s for one, two and four), which would not happen if the disk were the bottleneck. Decompressing PNG files is CPU-bound work, and it should be parallelised like any other part of the pipeline."),

  H2("6.2", "Conclusion"),
  P(`We built a large-scale image filtering pipeline that applies hand-written Gaussian, sharpening and Sobel filters to 10,000 chest X-rays, and parallelised it with OpenMP, MPI and hybrid MPI + OpenMP using both whole-image and strip decompositions. Every version produces exactly the same output as the sequential program. On a four-core laptop, the best configurations reduce the run time from ${T_SEQ} s to about 55 s, a 3.5× speedup that is close to what four physical cores and their hyper-threads allow.`),
  P("The experiments show that the decisive design choice is granularity. Coarse-grained distribution of independent images scales well under every programming model. Fine-grained decomposition is limited by the serial part of the pipeline, as Amdahl’s law predicts, and by per-image communication and synchronisation. Strip decomposition remains the right tool for a single image too large for one process, and in that setting the hybrid model, with fewer processes and more threads, gives a clear advantage over pure MPI."),
  H2("6.3", "Future work"),
  bullet("Overlap reading and computation in the strip version, for example by letting rank 0 decode the next image while the current one is being filtered, or by letting several ranks read in turn."),
  bullet("Run the batch and hybrid versions on a multi-node cluster, where communication travels over a network and the advantage of hybrid MPI + OpenMP should grow."),
  bullet("Vectorise and cache-block the convolution kernel to reduce the compute share further."),
];

// =============================================================================================
// REFERENCES
// =============================================================================================
const refItem = (n, text) => new Paragraph({ spacing: { after: 100 }, indent: { left: 480, hanging: 480 },
  children: [new TextRun({ text: `[${n}]\t`, font: HEAD, bold: true, color: NAVY, size: 20 }), ...runs(text, { size: 20 })],
  tabStops: [{ type: "left", position: 480 }] });
const refs = [
  new Paragraph({ heading: HeadingLevel.HEADING_1, pageBreakBefore: true, children: [new TextRun("References")] }),
  refItem(1, "X. Wang, Y. Peng, L. Lu, Z. Lu, M. Bagheri and R. M. Summers, “ChestX-ray8: Hospital-scale chest X-ray database and benchmarks on weakly-supervised classification and localization of common thorax diseases,” in *Proc. IEEE Conference on Computer Vision and Pattern Recognition (CVPR)*, 2017, pp. 2097–2106."),
  refItem(2, "G. M. Amdahl, “Validity of the single processor approach to achieving large scale computing capabilities,” in *Proc. AFIPS Spring Joint Computer Conference*, 1967, pp. 483–485."),
  refItem(3, "OpenMP Architecture Review Board, *OpenMP Application Programming Interface*, Version 5.2, 2021."),
  refItem(4, "Message Passing Interface Forum, *MPI: A Message-Passing Interface Standard*, Version 4.1, 2023."),
  refItem(5, "R. C. Gonzalez and R. E. Woods, *Digital Image Processing*, 4th ed. Pearson, 2018."),
  refItem(6, "OpenCV team, *Open Source Computer Vision Library*, https://opencv.org."),
];

// =============================================================================================
// DOCUMENT
// =============================================================================================
const numberingConfig = [
  { reference: "bullets", levels: [{ level: 0, format: LevelFormat.BULLET, text: "•", alignment: AlignmentType.LEFT,
      style: { paragraph: { indent: { left: 400, hanging: 260 } }, run: { color: ACCENT } } }] },
  ...numberingRefs.map((ref) => ({ reference: ref, levels: [{ level: 0, format: LevelFormat.DECIMAL, text: "%1.", alignment: AlignmentType.LEFT,
      style: { paragraph: { indent: { left: 400, hanging: 300 } }, run: { font: HEAD, bold: true, color: NAVY } } }] })),
];

const headerPara = new Paragraph({
  alignment: AlignmentType.RIGHT, spacing: { after: 0 },
  border: { bottom: { style: BorderStyle.SINGLE, size: 4, color: RULE, space: 6 } },
  children: [new TextRun({ text: "Hybrid Parallel Image Convolution", font: HEAD, size: 16, color: GRAY }),
    new TextRun({ text: "   ·   Group 18   ·   CS3006", font: HEAD, size: 16, color: GRAY })],
});
const footerPara = new Paragraph({ alignment: AlignmentType.CENTER,
  children: [new TextRun({ children: [PageNumber.CURRENT], font: HEAD, size: 18, color: GRAY })] });

const doc = new Document({
  creator: "Muhammad Yousuf, Arsalan Ahmed",
  title: "Hybrid Parallel Large-Scale Image Processing and Convolution Filtering",
  description: "PDC (CS3006) semester project report, Group 18",
  styles: {
    default: {
      document: { run: { font: BODY, size: 22, color: INK }, paragraph: { spacing: { after: 140, line: 288 } } },
      heading1: { run: { font: HEAD, size: 40, bold: true, color: NAVY },
        paragraph: { spacing: { before: 0, after: 280 }, keepNext: true, keepLines: true, outlineLevel: 0 } },
      heading2: { run: { font: HEAD, size: 27, bold: true, color: NAVY },
        paragraph: { spacing: { before: 320, after: 120 }, keepNext: true, keepLines: true, outlineLevel: 1 } },
      heading3: { run: { font: HEAD, size: 23, bold: true, color: NAVY },
        paragraph: { spacing: { before: 200, after: 80 }, keepNext: true, outlineLevel: 2 } },
    },
    paragraphStyles: [
      { id: "TOC1", name: "toc 1", basedOn: "Normal", next: "Normal", quickFormat: true,
        run: { font: HEAD, size: 22, bold: true, color: NAVY }, paragraph: { spacing: { before: 180, after: 60 } } },
      { id: "TOC2", name: "toc 2", basedOn: "Normal", next: "Normal", quickFormat: true,
        run: { font: HEAD, size: 21, color: INK }, paragraph: { indent: { left: 360 }, spacing: { before: 0, after: 40 } } },
    ],
  },
  numbering: { config: numberingConfig },
  sections: [{
    properties: { titlePage: true, page: { size: { width: 11906, height: 16838 }, margin: { top: 1440, bottom: 1300, left: 1440, right: 1440, header: 600, footer: 600 } } },
    headers: { default: new Header({ children: [headerPara] }), first: new Header({ children: [new Paragraph("")] }) },
    footers: { default: new Footer({ children: [footerPara] }), first: new Footer({ children: [new Paragraph("")] }) },
    children: [...cover, ...abstract, ...contents, ...intro, ...dataset, ...method, ...impl, ...results, ...concl, ...refs],
  }],
});

Packer.toBuffer(doc).then((buf) => { fs.writeFileSync(OUT, buf); console.log("wrote", OUT); });
