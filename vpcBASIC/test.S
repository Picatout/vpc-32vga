#include <p32xxxx.h>
#include "vm.h"
    
    .data
var1: .byte 1
var2: .byte 2
var3: .byte 3
var4: .byte 4
var5: .byte 5,6
    
.text
.align 2, 0xff
.set noreorder
.set macro

.global test_ulw1, test_ulw3, test_lwr1, test_lwr3
 
# .ent test_ulw1 
test_ulw1:
    lui t0,RAM_SEG
    ori t0,t0,var2
    ulw v0,0(t0)
    j ra
    nop
 # .end
    
# .ent test_ulw3    
test_ulw3:
    lui t0,RAM_SEG
    ori t0,t0,var3
    ulw v0,0(t0)
    j ra
    nop
 # .end    

# .ent test_lwr1
test_lwr1:
    lui t0,RAM_SEG
    ori t0,t0,var2
    lwr v0,0(t0)
    j ra
    and v0,v0,0xffff
 # .end
    
# .ent test_lwr3
test_lwr3:    
    lui t0,RAM_SEG
    ori t0,t0,var3
    lwr v0,0(t0)
    j ra
    and v0,v0,0xffff
#  .end