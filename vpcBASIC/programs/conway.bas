rem John Conway's game of life

dim cursor(8)=(
$20000000,
$20000000,
$20000000,
$f8000000,
$20000000,
$20000000,
$20000000,
$00000000)

const width=38
const height=18
const cell#=79
const empty#=32
const k_up=141
const k_down=142
const k_left=143
const k_right=144
const odd=684
dim void
dim g#(1368)
gen=0
src=0
des=odd

sub clear_grid()
local i
  for i=1 to ubound(g#)
    g#(i)=empty#
  next i
end sub

sub display_grid()
local x,y
 for x=1 to 38
  for y=1 to 18
   locate(y,x)
   putc g#((y-1)*width+x+src)
  next y
 next x
 locate(20,0)
 print gen ;
end sub 

sub set_grid()
local k,x,y
 x=width/2+1
 y=height/2+1
 cls
 locate(21,0)
 ? "<SPACE>,<C>,<ENTER>";
 gen=0
 src=0
 des=odd
 k=0 
 clear_grid(): ?k,x,y
 while k<>13
  locate(0,0)
  ? "x:",x," y:",y,"    ";
  locate(y,x)
  putc g#((y-1)*width+x)
  locate(y,x)
  k= key()
  select case k
  case k_up
    if y>1 then y=y-1 end if
  case k_down
    if y<height then y=y+1 end if
  case k_left
    if x>1 then x=x-1 end if
  case k_right
    if x<38 then x=x+1 end if
  case 32
    g#(width*(y-1)+x)=empty#
  case 67,99
    g#((y-1)*width+x)=cell#
  end select
  if k<>13 then k=0 end if
 wend
 cls
 ? "'q' quit, 'r' new grid";
end sub

func countn(x,y)
local n,x0,y0,ofs
 n=0
 for x0=max(x-1,1) to min(38,x+1)
  for y0=max(y-1,1) to min(18,y+1)
  if x0<>x or y0<>y then
   ofs=(y0-1)*width+x0+src
   if g#(ofs)=cell# then
    n=n+1
   end if
  end if
  next y0
 next x0
 return n 
end func

sub next_gen()
local n,x,y,ofs
  for x=1 to 38
   for y=1 to 18
    n=countn(x,y)
    ofs=(y-1)*width+x
    select case n
    case 2
     g#(ofs+des)=g#(ofs+src)
    case 3
     g#(ofs+des)=cell#
    case else ' 0,1,4,5,6,7,8
     g#(ofs+des)=empty#
    end select
   next y
  next x
  gen=gen+1
  src=odd-src
  des=odd-des
end sub

set_grid()
r=1
while r
  display_grid()
  next_gen()        
  select case tkey()
  case \q,\Q
   r=0
  case \r,\R
   set_grid()
  end select
wend
cls
                                                                                                                 