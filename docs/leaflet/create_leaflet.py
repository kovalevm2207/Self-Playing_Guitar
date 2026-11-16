"""Rebuild the print leaflet next to this script with ReportLab."""
from pathlib import Path
from reportlab.pdfgen import canvas
from reportlab.lib.colors import HexColor, Color, white
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import Paragraph
from reportlab.lib.styles import ParagraphStyle
from reportlab.graphics.barcode.qr import QrCodeWidget
from reportlab.graphics.shapes import Drawing
from reportlab.graphics import renderPDF

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent / 'self-playing-guitar-leaflet.pdf'
OUT.parent.mkdir(parents=True, exist_ok=True)
for name, filename in [('Text', 'arial.ttf'), ('Bold', 'arialbd.ttf')]:
    pdfmetrics.registerFont(TTFont(name, str(Path('C:/Windows/Fonts') / filename)))
W, H = A4
INK = HexColor('#182B34')
TEAL = HexColor('#006B64')
MUTED = HexColor('#4A5C64')
PALE = HexColor('#EFF5F4')
LINE = HexColor('#C9D6D5')
c = canvas.Canvas(str(OUT), pagesize=A4, pageCompression=1)
c.setTitle('Self-Playing Guitar | Самоиграющая гитара')
c.setAuthor('Ковалев Михаил, Козлов Тимофей')
c.setSubject('Лифлет проекта. Идея, принцип работы, текущие результаты и команда.')

def text(x, y, s, size=10, font='Text', color=INK):
    c.setFillColor(color)
    c.setFont(font, size)
    c.drawString(x*mm, H-y*mm, s)

def para(x, y, width, s, size=10.5, leading=14.5, color=INK):
    style = ParagraphStyle('body', fontName='Text', fontSize=size,
                           leading=leading, textColor=color)
    p = Paragraph(s, style)
    _, h = p.wrap(width*mm, H)
    p.drawOn(c, x*mm, H-y*mm-h)
    return h/mm

def rect(x,y,w,h,color):
    c.setFillColor(color)
    c.rect(x*mm,H-(y+h)*mm,w*mm,h*mm,stroke=0,fill=1)

def rule(y):
    c.setStrokeColor(LINE)
    c.setLineWidth(.6)
    c.line(15*mm,H-y*mm,195*mm,H-y*mm)

text(15,18,'ФАБРИКА  /  ИНЖЕНЕРНЫЙ ПРОЕКТ',9,'Bold',TEAL)
text(15,35,'Self-Playing Guitar',31,'Bold')
text(15,47,'Самоиграющая акустическая гитара',17,'Bold',TEAL)
para(15,55,178,'Разрабатываем устройство для автоматической игры на акустической гитаре. '
     'Цель: создать систему, способную автоматически исполнять мелодию на одной струне.',11.5,16)

rule(79)
for x, big, small in [(15,'1 струна','цель текущего этапа'),(78,'Каретка','перемещение вдоль грифа'),
                       (141,'Зажим','прижатие струны к ладу')]:
    text(x,90,big,18,'Bold',TEAL)
    text(x,97,small,9.5,color=MUTED)

text(15,111,'КАК УСТРОЕН МЕХАНИЗМ',10,'Bold',TEAL)
c.drawImage(str(ROOT/'Эскизы/7835cd99-7a89-4ed8-9cad-218d37764df3.jpg'),
            15*mm,H-178*mm,width=94*mm,height=64*mm,
            preserveAspectRatio=True,anchor='c',mask='auto')
text(15,184,'Эскиз каретки и механизма зажима',8.2,color=MUTED)

for y,n,title,body in [
    (117,'01','Перемещение каретки','Ременная передача перемещает каретку по направляющим вдоль грифа к нужному ладу.'),
    (141,'02','Зажим струны','Соленоид через трос приводит в действие «палец», прижимающий струну к ладу.'),
    (165,'03','Извлечение звука','Для исполнения мелодии планируется механизм щипка с медиатором.')]:
    text(116,y,n,10,'Bold',TEAL)
    text(126,y,title,11,'Bold')
    para(126,y+3,69,body,9.3,12)

rule(193)
text(15,204,'ТЕКУЩИЙ РЕЗУЛЬТАТ',10,'Bold',TEAL)
para(15,209,83,'Готов стенд на одну струну с перемещением каретки '
     'и механизмом зажима. Разработаны схема подключения электроники, '
     'бета-версия приложения и 3D-модели большинства деталей.',10,13.5)
text(112,204,'СЛЕДУЮЩИЙ ЭТАП',10,'Bold',TEAL)
para(112,209,83,'Разработать механизм щипка, завершить разработку программного '
     'обеспечения и довести проект до полностью рабочего состояния.',10,13.5)

rule(258)
text(15,266,'КОМАНДА',8,'Bold',TEAL)
text(15,273,'Ковалев Михаил · Козлов Тимофей',10,'Bold')
text(15,279,'МФТИ · Б01-502',9,color=MUTED)
text(15,285,'Ментор: Тавитов Азамат Георгиевич',9,color=MUTED)

url='https://github.com/kovalevm2207/Self-Playing_Guitar'
q=QrCodeWidget(url,barLevel='M')
b=q.getBounds(); sz=26*mm
d=Drawing(sz,sz,transform=[sz/(b[2]-b[0]),0,0,sz/(b[3]-b[1]),0,0])
d.add(q)
renderPDF.draw(d,c,169*mm,H-285*mm)
c.linkURL(url,(169*mm,H-285*mm,195*mm,H-259*mm),relative=0)
text(112,266,'КОД И МАТЕРИАЛЫ',8,'Bold',TEAL)
text(112,273,'GitHub / QR-код',9.5)
text(112,279,'Связь: @kovalevm2207',8.5,color=MUTED)
text(112,285,'@timoko3',8.5,color=MUTED)
c.linkURL('https://t.me/kovalevm2207',(112*mm,H-281*mm,164*mm,H-275*mm),relative=0)
c.linkURL('https://t.me/timoko3',(112*mm,H-287*mm,138*mm,H-281*mm),relative=0)
c.showPage()
c.save()
print(OUT)
