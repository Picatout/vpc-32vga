cls
? "test de recursion"
? "carre fractal"
t0=ticks()+2000
while ticks()<>t0 and not tkey() wend
vgacls
lm=(480-3*80)/2
sub fractal(x,y,n) 
local x0,y0
 if n<=3 then 
  for x0=x to x+2*n step n
   for y0=y to y+2*n step n 
    if x0<>x+n or y0<>y+n then 
        box(lm+x0,y0,n,n) 
    end if       
   next y0
  next x0
 else
  for x0=x to x+2*n step n
   for y0=y to y+2*n step n
    if x0<>x+n or y0<>y+n then 
        fractal(x0,y0,n/3)
    end if
   next y0
  next x0
 end if 
end sub
fractal(0,0,80)
while not tkey() wend
vgacls

                                                                                                       