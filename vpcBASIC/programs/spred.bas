REM editeur de sprite

dim name$
dim c as byte
dim w as byte
dim h as byte
dim spr#(512)
dim c#(8)=($f1,$1f,
           $1f,$f1,
           $1f,$f1,
           $f1,$1f)
dim xo,yo,x,y
const psize=4
const xres=480
const yres=240
c=15
cls
input "sprite name", name$
input "sprite width", w
input "sprite height", h


sub draw_bounds()
 cls
 xo=(xres-psize*w)/2
 yo=(yres-psize*h)/2
 rect(xo-1,yo-1,psize*w+2,psize*h+2)
 x=w/2
 y=h/2
end sub

sub prtxy()
  locate(0,0)
  print "x:",x," y:",y ;
end sub

sub clrln(n)
    local i
    locate(n,0)
    for i=0 to 79:putc 32 : next i
    locate(n,0)
end sub

sub put_big_pixel(x,y,c)
    local i,j
    x=xo+x*psize
    y=yo+y*psize
    for i=0 to psize-1
        for j=0 to psize-1
            pset(x+i,y+j,c)
        next j
    next i
end sub

sub save_sprite()
  local io_code
  clrln(20):? "saving sprite"
  srwrite(0,@w,1)
  srwrite(1,@h,1)
  srwrite(2,@spr#,w*h/2)
  clrln(20)
  io_code=srsave(0,name$,w*h/2+2)
  if not io_code then
    ? name$ ,"saved"
    clrln(21)
    ? w,"x",h,"pixels";
  else
    ? "failed to save ",name$, "exit code: ",io_code
  end if
end sub

func get_pixel(x,y)
    local i,c#
    i=(y*w+x)/2+1
    if btest(x,0) then
        c#=spr#(i)/16
    else
        c#=spr#(i)%16
    end if
    return c#
end func

sub fill_canevas()
local x,y,c#
  for x=0 to w-1
   for y=0 to h-1
    c#=get_pixel(x,y)
    put_big_pixel(x,y,c#)
    pset(xo-w-2+x,yo-h-2+y,c#)
   next y
  next x
end sub

sub wipe_canevas()
    local i,j
    for i=xo to xo+w-1
        for j=yo to yo+h-1
            pset(i,j,0)
        next j
    next i
    for i=1 to 128:spr#(i)=0:next i
end sub

sub init_state()
    cls
    x=w/2
    y=h/2
    prtxy()
    draw_bounds()
    fill_canevas()
end sub

sub load_sprite()
local size
 size=srload(0,name$)
 if size>0 then
   srread(0,@w,1)
   srread(1,@h,1)
   srread(2,@spr#,w*h/2)
   init_state()
   clrln(20)
   ? name$, "loaded"
   ? "width",w,"height",h ;
 else
   clrln(20)
   ? "load failed, exit code is",size;
 end if
end sub

sub let_pixel(x,y,c)
local idx
  idx=(y*w+x)/2+1
  if btest(x,0) then 'impair
   spr#(idx)=spr#(idx)%16+16*c
  else ' pair
   spr#(idx)=spr#(idx)/16*16+c
  end if
end sub


init_state()
dim leave
while not leave
 prtxy()
 sprite(xo+x*psize,yo+y*psize,psize,psize,@c#)
 sleep(20)
 sprite(xo+x*psize,yo+y*psize,psize,psize,@c#)
 k=tkey()
 select case k
 case \0,\1,\2,\3,\4,\5,\6,\7,\8,\9
  c=k-\0
 case \a,\b,\c,\d,\e,\f
  c=k-\a+10
 case 32
  put_big_pixel(x,y,c)
  pset(xo-w-2+x,yo-h-2+y,c)
  let_pixel(x,y,c)
 case 141 'UP
  if y>0 then y=y-1 end if
 case 142 'DOWN
  if y<h-1 then y=y+1 end if
 case 143 'LEFT
  if x>0 then x=x-1 end if
 case 144 'RIGHT
  if x<h-1 then x=x+1 end if
 case \g
  fill_canevas()
 case \w
    wipe_canevas()
    fill_canevas()
 case \q ' quit
  leave=1
 case \s ' save
  save_sprite()
 case \l' load
  load_sprite()
 end select
wend
cls
                                                           