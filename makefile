CC = wcc
AS = wasm
LD = wlink
CL = wcl

CFLAGS = -3 -bt=dos -i=lib866d -DDEBUG
LDFLAGS = SYSTEM DOS

OBJ = lib866d\vgacon.obj lib866d\sys.obj lib866d\util.obj lib866d\args.obj gusdiag.obj

all : GUSDIAG.EXE

GUSDIAG.EXE : $(OBJ)
    $(LD) $(LDFLAGS) NAME GUSDIAG FILE { lib866d\*.obj *.obj }

.c: lib866d
.c.obj : .AUTODEPEND
        $(CC) $(CFLAGS) -fo=$@ $<

