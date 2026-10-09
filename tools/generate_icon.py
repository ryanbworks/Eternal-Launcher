from pathlib import Path
from PIL import Image, ImageDraw
im=Image.new('RGBA',(256,256),(13,14,20,255))
draw=ImageDraw.Draw(im)
scale=4.25
offset=26
pts=[(8,4),(35,4),(43,12),(19,12),(13,18),(34,18),(34,26),(13,26),(19,36),(43,36),(35,44),(8,44),(3,35),(3,13)]
gate=[(39,17),(46,24),(39,31),(35,27),(38,24),(35,21)]
for shape in [pts,gate]:
    draw.polygon([(offset+x*scale,offset+y*scale) for x,y in shape],fill=(165,130,255,255))
im.save('assets/eternal.ico',sizes=[(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)])
im.save('assets/eternal-icon.png')
