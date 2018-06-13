rem snake game
cls
dim hup=\V,hdn=\m,hlt=\>,hrt=\<
dim head,snake#(256),slen
dim collision=0
dim food(2)
const LEFT=0,RIGHT=33,TOP=1,BOTTOM=28

sub draw_walls()
  local x,y
    for x=LEFT to RIGHT 
      locate(TOP,x)
      putc \X
      locate(BOTTOM,x)
      putc \X
    next x
    for y=TOP+1 to BOTTOM-1
      locate(y,LEFT)
      putc \X
      locate(y,RIGHT)
      putc \X
    next y
end sub

sub draw_head()
    locate(snake#(2),snake#(1))
    select case head
    case 0
      putc hup
    case 1
      putc hrt
    case 2
      putc hdn
    case 3
      putc hlt
    end select
end sub

sub print_slen()
   locate(0,0)
   ? "length:",slen,"                ";
end sub

sub draw_snake()
 local x,y,i
  
 draw_head()
 for i=2 to slen
   x=snake#(2*i) 
   y=snake#(2*i+1)
   locate(y,x)
   putc \O
 next i
end sub

sub draw_food()
  locate(food(2),food(1))
  putc \O
end sub

sub new_food()
  food(1)=LEFT+1+rnd()%(RIGHT-LEFT-1)
  food(2)=TOP+1+rnd()%(BOTTOM-TOP-1)
end sub

sub grow_snake(x,y)
  if snake#(1)=food(1) and
     snake#(2)=food(2) then
     sound(500,30) 
     slen=slen+1 
     snake#(2*slen)=x
     snake#(2*slen+1)=y
     new_food()
     print_slen()
  end if
end sub

sub move_snake()
  local x,y,x1,y1,i

 x=snake#(1)
 y=snake#(2)
 collision=0
 select case head
 case 0 'up
  snake#(2)=snake#(2)-1
  if snake#(2)=TOP then
    collision=1
  end if
 case 1 ' right
  snake#(1)=snake#(1)+1
  if snake#(1)=RIGHT then
    collision=1
  end if
 case 2 ' down
  snake#(2)=snake#(2)+1
  if snake#(2)=BOTTOM then
   collision=1
  end if
 case 3 ' left
  snake#(1)=snake#(1)-1
  if snake#(1)=LEFT then
    collision=1
  end if
 end select
 draw_head()
 for i=2 to slen
   x1=snake#(2*i)
   y1=snake#(2*i+1)
   snake#(2*i)=x
   snake#(2*i+1)=y
   locate(y,x)
   putc \O
   if x=snake#(1) and y=snake#(2) then
     collision=1
   end if
   x=x1 : y=y1
 next i
 locate(y,x)
 putc 32
 if not collision then
   grow_snake(x,y)
 end if
end sub

sub snake_init()
  head = 1
  slen = 3
  snake#(1)=(RIGHT-LEFT)/2+1
  snake#(2)=(BOTTOM-TOP)/2+1
  snake#(3)=snake#(1)-1
  snake#(4)=snake#(2)
  snake#(5)=snake#(1)-2
  snake#(6)=snake#(2)
  draw_snake()
  print_slen()
end sub

sub snake()
 draw_walls()
 snake_init()
 new_food()
 collision=0
 while not collision
  draw_food()
  locate(0,0)
  sleep(100)
  select case tkey()
  case 143 'left
    head=(head-1)
    if head<0 then head=3 end if
  case 144 'right
    head=(head+1)%4
  end select  
  move_snake()
 wend
end sub

dim k=0

while k<>\q
  snake()
  while slen
    sound(500+40*slen,200)
    locate(snake#(2*slen+1),snake#(2*slen))
    putc 32
    slen=slen-1
  wend
  cls
  locate(0,0)
  ? "<q> to Quit, other next"
  k=waitkey()
wend
cls
