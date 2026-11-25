from pathlib import Path
import re
import sys

from docx import Document
from docx.enum.section import WD_SECTION
from docx.enum.table import WD_CELL_VERTICAL_ALIGNMENT, WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_BREAK, WD_LINE_SPACING
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Cm, Inches, Pt, RGBColor


ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / (sys.argv[1] if len(sys.argv) > 1 else "protocolo_secciones_1_a_7.md")
OUTPUT = ROOT / (sys.argv[2] if len(sys.argv) > 2 else "Protocolo_Proyecto_Final.docx")


def set_cell_shading(cell, fill: str) -> None:
    tc_pr = cell._tc.get_or_add_tcPr()
    shading = OxmlElement("w:shd")
    shading.set(qn("w:fill"), fill)
    tc_pr.append(shading)


def set_cell_margins(cell, top=90, start=100, bottom=90, end=100) -> None:
    tc = cell._tc
    tc_pr = tc.get_or_add_tcPr()
    tc_mar = tc_pr.first_child_found_in("w:tcMar")
    if tc_mar is None:
        tc_mar = OxmlElement("w:tcMar")
        tc_pr.append(tc_mar)
    for margin, value in (("top", top), ("start", start), ("bottom", bottom), ("end", end)):
        node = tc_mar.find(qn(f"w:{margin}"))
        if node is None:
            node = OxmlElement(f"w:{margin}")
            tc_mar.append(node)
        node.set(qn("w:w"), str(value))
        node.set(qn("w:type"), "dxa")


def set_repeat_table_header(row) -> None:
    tr_pr = row._tr.get_or_add_trPr()
    tbl_header = OxmlElement("w:tblHeader")
    tbl_header.set(qn("w:val"), "true")
    tr_pr.append(tbl_header)


def set_spanish_language(run) -> None:
    r_pr = run._element.get_or_add_rPr()
    lang = r_pr.find(qn("w:lang"))
    if lang is None:
        lang = OxmlElement("w:lang")
        r_pr.append(lang)
    lang.set(qn("w:val"), "es-MX")


def add_inline_markdown(paragraph, text: str) -> None:
    """Add the small subset of Markdown used by the source document."""
    pieces = re.split(r"(\*\*.*?\*\*)", text)
    for piece in pieces:
        if not piece:
            continue
        if piece.startswith("**") and piece.endswith("**"):
            run = paragraph.add_run(piece[2:-2])
            run.bold = True
        else:
            run = paragraph.add_run(piece)
        set_spanish_language(run)


def configure_document(document: Document) -> None:
    section = document.sections[0]
    section.page_width = Inches(8.5)
    section.page_height = Inches(11)
    section.top_margin = Cm(2.54)
    section.bottom_margin = Cm(2.54)
    section.left_margin = Cm(3.0)
    section.right_margin = Cm(2.5)

    styles = document.styles

    normal = styles["Normal"]
    normal.font.name = "Arial"
    normal.font.size = Pt(12)
    normal._element.rPr.rFonts.set(qn("w:eastAsia"), "Arial")
    normal.paragraph_format.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY
    normal.paragraph_format.line_spacing_rule = WD_LINE_SPACING.ONE_POINT_FIVE
    normal.paragraph_format.space_after = Pt(6)
    normal.paragraph_format.widow_control = True

    heading_1 = styles["Heading 1"]
    heading_1.font.name = "Arial"
    heading_1.font.size = Pt(14)
    heading_1.font.bold = True
    heading_1.font.color.rgb = RGBColor(0, 0, 0)
    heading_1._element.rPr.rFonts.set(qn("w:eastAsia"), "Arial")
    heading_1.paragraph_format.space_before = Pt(14)
    heading_1.paragraph_format.space_after = Pt(6)
    heading_1.paragraph_format.keep_with_next = True

    heading_2 = styles["Heading 2"]
    heading_2.font.name = "Arial"
    heading_2.font.size = Pt(12)
    heading_2.font.bold = True
    heading_2.font.color.rgb = RGBColor(0, 0, 0)
    heading_2._element.rPr.rFonts.set(qn("w:eastAsia"), "Arial")
    heading_2.paragraph_format.space_before = Pt(10)
    heading_2.paragraph_format.space_after = Pt(4)
    heading_2.paragraph_format.keep_with_next = True

    for style_name in ("List Bullet", "List Number"):
        style = styles[style_name]
        style.font.name = "Arial"
        style.font.size = Pt(12)
        style._element.rPr.rFonts.set(qn("w:eastAsia"), "Arial")
        style.paragraph_format.line_spacing_rule = WD_LINE_SPACING.ONE_POINT_FIVE
        style.paragraph_format.space_after = Pt(3)

    core = document.core_properties
    core.title = "Protocolo de Proyecto Final"
    core.subject = "Generador procedural de jardines y entornos naturales tridimensionales"
    core.author = "[Integrantes del equipo]"
    core.keywords = "Graficación por Computadora, Unity, generación procedural, entorno 3D"


def add_table(document: Document, rows: list[list[str]]) -> None:
    table = document.add_table(rows=1, cols=len(rows[0]))
    table.style = "Table Grid"
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    table.autofit = False

    widths = [Inches(2.55), Inches(3.95)]
    for col_index, value in enumerate(rows[0]):
        cell = table.rows[0].cells[col_index]
        cell.width = widths[col_index]
        cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
        set_cell_shading(cell, "D9EAD3")
        set_cell_margins(cell)
        paragraph = cell.paragraphs[0]
        paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
        paragraph.paragraph_format.line_spacing = 1.0
        paragraph.paragraph_format.space_after = Pt(0)
        run = paragraph.add_run(value)
        run.bold = True
        run.font.name = "Arial"
        run.font.size = Pt(10)
        set_spanish_language(run)

    set_repeat_table_header(table.rows[0])

    for source_row in rows[1:]:
        cells = table.add_row().cells
        for col_index, value in enumerate(source_row):
            cell = cells[col_index]
            cell.width = widths[col_index]
            cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
            set_cell_margins(cell)
            paragraph = cell.paragraphs[0]
            paragraph.alignment = WD_ALIGN_PARAGRAPH.LEFT
            paragraph.paragraph_format.line_spacing = 1.0
            paragraph.paragraph_format.space_after = Pt(0)
            run = paragraph.add_run(value)
            run.font.name = "Arial"
            run.font.size = Pt(10)
            set_spanish_language(run)

    document.add_paragraph()


def build_document() -> None:
    source_lines = SOURCE.read_text(encoding="utf-8").splitlines()
    document = Document()
    configure_document(document)

    index = 0
    while index < len(source_lines):
        raw = source_lines[index]
        line = raw.strip()

        if not line:
            index += 1
            continue

        if line.startswith("|-"):
            index += 1
            continue

        if line.startswith("|"):
            table_rows = []
            while index < len(source_lines) and source_lines[index].strip().startswith("|"):
                current = source_lines[index].strip()
                if not re.fullmatch(r"\|[\s|:-]+\|", current):
                    values = [value.strip() for value in current.strip("|").split("|")]
                    table_rows.append(values)
                index += 1
            if table_rows:
                add_table(document, table_rows)
            continue

        if line.startswith("# "):
            paragraph = document.add_paragraph()
            paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
            paragraph.paragraph_format.space_after = Pt(12)
            paragraph.paragraph_format.keep_with_next = True
            run = paragraph.add_run(line[2:])
            run.bold = True
            run.font.name = "Arial"
            run.font.size = Pt(16)
            set_spanish_language(run)
        elif line.startswith("## "):
            document.add_heading(line[3:], level=1)
        elif line.startswith("### "):
            document.add_heading(line[4:], level=2)
        elif re.match(r"^\d+\.\s", line):
            text = re.sub(r"^\d+\.\s+", "", line)
            paragraph = document.add_paragraph(style="List Number")
            paragraph.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY
            add_inline_markdown(paragraph, text)
        elif line.startswith("- "):
            paragraph = document.add_paragraph(style="List Bullet")
            paragraph.alignment = WD_ALIGN_PARAGRAPH.LEFT
            add_inline_markdown(paragraph, line[2:])
        else:
            paragraph = document.add_paragraph()
            paragraph.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY
            paragraph.paragraph_format.first_line_indent = Cm(1.25)
            if line.startswith("**") and ":**" in line:
                paragraph.paragraph_format.first_line_indent = Cm(0)
            add_inline_markdown(paragraph, line.rstrip())

        index += 1

    document.save(OUTPUT)
    print(OUTPUT)


if __name__ == "__main__":
    build_document()
