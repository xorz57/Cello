#include "Cello.h"

static const char* Table_Name(void) {
  return "Table";
}

/* TODO */
static const char* Table_Brief(void) {
  return "";
}

/* TODO */
static const char* Table_Description(void) {
  return "";
}

/* TODO */
static const char* Table_Examples(void) {
  return "";
}

/* TODO */
static const char* Table_Methods(void) {
  return "";
}

struct Table {
  var data;
  var ktype;
  var vtype;
  size_t ksize;
  size_t vsize;
  size_t nslots;
  size_t nitems;
  var sspace0;
  var sspace1;
};

enum {
  TABLE_PRIMES_COUNT = 24
};

static const size_t Table_Primes[TABLE_PRIMES_COUNT] = {
  0,       1,       5,       11,
  23,      53,      101,     197,
  389,     683,     1259,    2417,
  4733,    9371,    18617,   37097,
  74093,   148073,  296099,  592019,
  1100009, 2200013, 4400021, 8800019
};

static const double Table_Load_Factor = 0.9;

static size_t Table_Ideal_Size(size_t size) {
  size = (size_t)((double)(size+1) / Table_Load_Factor);
  for (size_t i = 0; i < TABLE_PRIMES_COUNT; i++) {
    if (Table_Primes[i] >= size) { return Table_Primes[i]; }
  }
  size_t last = Table_Primes[TABLE_PRIMES_COUNT-1];
  for (size_t i = 0;; i++) {
    if (last * i >= size) { return last * i; }
  }
}

static size_t Table_Step(struct Table* t) {
  return
    sizeof(uint64_t) + 
    sizeof(struct CelloHeader) + t->ksize + 
    sizeof(struct CelloHeader) + t->vsize;
}

static uint64_t Table_Key_Hash(struct Table* t, uint64_t i) {
  return *(uint64_t*)((char*)t->data + i * Table_Step(t));
}

static var Table_Key(struct Table* t, uint64_t i) {
  return (char*)t->data + i * Table_Step(t) +
    sizeof(uint64_t) + 
    sizeof(struct CelloHeader);  
}

static var Table_Val(struct Table* t, uint64_t i) {
  return (char*)t->data + i * Table_Step(t) +
    sizeof(uint64_t) + 
    sizeof(struct CelloHeader) + 
    t->ksize + 
    sizeof(struct CelloHeader);  
}

static uint64_t Table_Probe(struct Table* t, uint64_t i, uint64_t h) {
  uint64_t v = i - (h-1);
  if (v < 0) {
    v = t->nslots + v;
  }
  return v;
}

static void Table_Set(var self, var key, var val);
static void Table_Set_Move(var self, var key, var val, var move);

static size_t Table_Size_Round(size_t s) {
  return ((s + sizeof(var) - 1) / sizeof(var)) * sizeof(var);
}

static var Table_New(var self, var args) {
  
  struct Table* t = self;
  t->ktype = cast(get(args, $(Int, 0)), Type);
  t->vtype = cast(get(args, $(Int, 1)), Type);
  t->ksize = Table_Size_Round(size(t->ktype));
  t->vsize = Table_Size_Round(size(t->vtype));
  
  size_t nargs = len(args);
  if (nargs % 2 isnt 0) {
    return throw(FormatError, 
      "Received non multiple of two argument count to Table constructor.");
  }
  
  t->nslots = Table_Ideal_Size((nargs-2)/2);
  t->nitems = 0;
  
  if (t->nslots is 0) {
    t->data = None;
    return self;
  }
  
  t->data = calloc(t->nslots, Table_Step(t));
  t->sspace0 = calloc(1, Table_Step(t));
  t->sspace1 = calloc(1, Table_Step(t));
  
#if CELLO_MEMORY_CHECK == 1
  if (t->data is None or t->sspace0 is None or t->sspace1 is None) {
    throw(OutOfMemoryError, "Cannot allocate Table, out of memory!");
  }
#endif
  
  for(size_t i = 0; i < (nargs-2)/2; i++) {
    var key = get(args, $(Int, 2+(i*2)+0));
    var val = get(args, $(Int, 2+(i*2)+1));
    Table_Set_Move(t, key, val, False);
  }
  
  return self;
}

static var Table_Del(var self) {
  struct Table* t = self;  
  
  for (size_t i = 0; i < t->nslots; i++) {
    if (Table_Key_Hash(t, i) isnt 0) {
      destruct(Table_Key(t, i));
      destruct(Table_Val(t, i));
    }
  }
  
  free(t->data);
  free(t->sspace0);
  free(t->sspace1);
  
  return self;
}

static size_t Table_Size(void) {
  return sizeof(struct Table);
}

static void Table_Clear(var self) {
  struct Table* t = self;
  
  for (size_t i = 0; i < t->nslots; i++) {
    if (Table_Key_Hash(t, i) isnt 0) {
      destruct(Table_Key(t, i));
      destruct(Table_Val(t, i));
    }
  }
  
  free(t->data);
  
  t->nslots = 0;
  t->nitems = 0;
  t->data = None;
  
}

static var Table_Assign(var self, var obj) {
  struct Table* t = self;  
  Table_Clear(t);
  
  t->nitems = 0;
  t->nslots = Table_Ideal_Size(len(obj));
  
  if (t->nslots is 0) {
    t->data = None;
    return self;
  }
  
  t->data = calloc(t->nslots, Table_Step(t));
  
#if CELLO_MEMORY_CHECK == 1
  if (t->data is None) {
    throw(OutOfMemoryError, "Cannot allocate Table, out of memory!");
  }
#endif
  
  foreach(key in obj) {
    Table_Set_Move(t, key, get(obj, key), False);
  }
  
  return self;
  
}

static var Table_Copy(var self) {
  struct Table* t = self;
  
  var r = new(Table, t->ktype, t->vtype);
  
  for (size_t i = 0; i < t->nslots; i++) {
    if (Table_Key_Hash(t, i) isnt 0) {
      Table_Set(r, Table_Key(t, i), Table_Val(t, i));
    }
  }
  
  return r;
}

static var Table_Mem(var self, var key);
static var Table_Get(var self, var key);

static var Table_Eq(var self, var obj) {
  
  foreach (key in obj) {
    if (not Table_Mem(self, key)) { return False; }
    if_neq(get(obj, key), Table_Get(self, key)) { return False; }
  }
	
  foreach (key in self) {
    if (not mem(obj, key)) { return False; }
    if_neq(get(obj, key), Table_Get(self, key)) { return False; }
  }
	
  return True;
}

static size_t Table_Len(var self) {
  struct Table* t = self;
  return t->nitems;
}

static uint64_t Table_Swapspace_Hash(struct Table* t, var space) {
  return *((uint64_t*)space);
}

static var Table_Swapspace_Key(struct Table* t, var space) {
  return (char*)space + sizeof(uint64_t) + sizeof(struct CelloHeader);
}

static var Table_Swapspace_Val(struct Table* t, var space) {
  return (char*)space + sizeof(uint64_t) + sizeof(struct CelloHeader) +
    t->ksize + sizeof(struct CelloHeader); 
}

static void Table_Set_Move(var self, var key, var val, var move) {
  
  struct Table* t = self;
  key = cast(key, t->ktype);
  val = cast(val, t->vtype);
  
  uint64_t i = hash(key) % t->nslots;
  uint64_t j = 0;
  
  memset(t->sspace0, 0, Table_Step(t));
  memset(t->sspace1, 0, Table_Step(t));
  
  if (move) {
      
    uint64_t ihash = i+1;
    memcpy((char*)t->sspace0, &ihash, sizeof(uint64_t));
    memcpy((char*)t->sspace0 + sizeof(uint64_t),
      (char*)key - sizeof(struct CelloHeader),
      t->ksize + sizeof(struct CelloHeader));
    memcpy((char*)t->sspace0 + sizeof(uint64_t) +
      sizeof(struct CelloHeader) + t->ksize, 
      (char*)val - sizeof(struct CelloHeader),
      t->vsize + sizeof(struct CelloHeader));
  
  } else {
        
    struct CelloHeader* khead = (struct CelloHeader*)
      ((char*)t->sspace0 + sizeof(uint64_t));
    struct CelloHeader* vhead = (struct CelloHeader*)
      ((char*)t->sspace0 + sizeof(uint64_t) 
      + sizeof(struct CelloHeader) + t->ksize);
    
    CelloHeader_Init(khead, t->ktype, CelloDataAlloc);
    CelloHeader_Init(vhead, t->vtype, CelloDataAlloc);
    
    uint64_t ihash = i+1;
    memcpy((char*)t->sspace0, &ihash, sizeof(uint64_t)); 
    assign((char*)t->sspace0 + sizeof(uint64_t) + sizeof(struct CelloHeader), key);
    assign((char*)t->sspace0 + sizeof(uint64_t) + sizeof(struct CelloHeader)
      + t->ksize + sizeof(struct CelloHeader), val);
  }
  
  while (True) {
    
    uint64_t h = Table_Key_Hash(t, i);
    if (h is 0) {
      memcpy((char*)t->data + i * Table_Step(t), t->sspace0, Table_Step(t));
      t->nitems++;
      return;
    }
    
    if_eq(Table_Key(t, i), Table_Swapspace_Key(t, t->sspace0)) {
      destruct(Table_Key(t, i));
      destruct(Table_Val(t, i));
      memcpy((char*)t->data + i * Table_Step(t), t->sspace0, Table_Step(t));
      return;
    }
    
    uint64_t p = Table_Probe(t, i, h);
    if (j >= p) {
      memcpy((char*)t->sspace1, (char*)t->data + i * Table_Step(t), Table_Step(t));
      memcpy((char*)t->data + i * Table_Step(t), (char*)t->sspace0, Table_Step(t));
      memcpy((char*)t->sspace0, (char*)t->sspace1, Table_Step(t));
      j = p;
    }
    
    i = (i+1) % t->nslots;
    j++;
  }
  
}

static void Table_Rehash(struct Table* t, size_t new_size) {
  
  var old_data = t->data;
  size_t old_size = t->nslots;
  
  t->nslots = new_size;
  t->nitems = 0;
  t->data = calloc(t->nslots, Table_Step(t));
  
#if CELLO_MEMORY_CHECK == 1
  if (t->data is None) {
    throw(OutOfMemoryError, "Cannot allocate Table, out of memory!");
  }
#endif
  
  for (size_t i = 0; i < old_size; i++) {
    
    uint64_t h = *(uint64_t*)((char*)old_data + i * Table_Step(t));
    
    if (h isnt 0) {
      var key = (char*)old_data + i * Table_Step(t) +
        sizeof(uint64_t) + sizeof(struct CelloHeader);
      var val = (char*)old_data + i * Table_Step(t) +
        sizeof(uint64_t) + sizeof(struct CelloHeader) + 
        t->ksize + sizeof(struct CelloHeader);
      Table_Set_Move(t, key, val, True);
    }
    
  }
  
  free(old_data);
}

static void Table_Resize_More(struct Table* t) {
  size_t new_size = Table_Ideal_Size(t->nitems);  
  size_t old_size = t->nslots;
  if (new_size > old_size) { Table_Rehash(t, new_size); }
}

static void Table_Resize_Less(struct Table* t) {
  size_t new_size = Table_Ideal_Size(t->nitems);  
  size_t old_size = t->nslots;
  if (new_size < old_size) { Table_Rehash(t, new_size); }
}

static var Table_Mem(var self, var key) {
  struct Table* t = self;
  key = cast(key, t->ktype);
  
  if (t->nslots is 0) { return False; }
  
  uint64_t i = hash(key) % t->nslots;
  uint64_t j = 0;
  
  while (True) {
    
    uint64_t h = Table_Key_Hash(t, i);
    if (h is 0 or j > Table_Probe(t, i, h)) {
      return False;
    }
    
    if_eq(Table_Key(t, i), key) {
      return True;
    }
    
    i = (i+1) % t->nslots; j++;
  }
  
  return False;
}

static void Table_Rem(var self, var key) {
  struct Table* t = self;
  key = cast(key, t->ktype);
  
  if (t->nslots is 0) {
    throw(KeyError, "Key %$ not in Table!", key);
  }
  
  uint64_t i = hash(key) % t->nslots;
  uint64_t j = 0;
  
  while (True) {
    
    uint64_t h = Table_Key_Hash(t, i);
    if (h is 0 or j > Table_Probe(t, i, h)) {
      throw(KeyError, "Key %$ not in Table!", key);
    }
    
    if_eq(Table_Key(t, i), key) {
      
      destruct(Table_Key(t, i));
      destruct(Table_Val(t, i));
      memset((char*)t->data + i * Table_Step(t), 0, Table_Step(t));
      
      while (True) {
        
        uint64_t ni = (i+1) % t->nslots;
        uint64_t nh = Table_Key_Hash(t, ni);
        if (nh isnt 0 and Table_Probe(t, ni, nh) > 0) {
          memcpy(
            (char*)t->data + i * Table_Step(t),
            (char*)t->data + ni * Table_Step(t),
            Table_Step(t));
          memset((char*)t->data + ni * Table_Step(t), 0, Table_Step(t));
          i = ni;
        } else {
          break;
        }
        
      }
      
      t->nitems--;
      Table_Resize_Less(t);
      return;
    }
    
    i = (i+1) % t->nslots; j++;
  }
  
}

static var Table_Get(var self, var key) {
  struct Table* t = self;
  key = cast(key, t->ktype);
  
  if (t->nslots is 0) {
    throw(KeyError, "Key %$ not in Table!", key);
  }
  
  uint64_t i = hash(key) % t->nslots;
  uint64_t j = 0;
  
  while (True) {

    uint64_t h = Table_Key_Hash(t, i);
    if (h is 0 or j > Table_Probe(t, i, h)) {
      throw(KeyError, "Key %$ not in Table!", key);
    }
    
    if_eq(Table_Key(t, i), key) {
      return Table_Val(t, i);
    }
    
    i = (i+1) % t->nslots; j++;
  }
  
  return Undefined;
}

static void Table_Set(var self, var key, var val) {
  Table_Set_Move(self, key, val, False);
  Table_Resize_More(self);
}

static var Table_Iter_Init(var self) {
  struct Table* t = self;
  if (t->nitems is 0) { return Terminal; }
  
  for (size_t i = 0; i < t->nslots; i++) {
    if (Table_Key_Hash(t, i) isnt 0) {
      return Table_Key(t, i);
    }
  }
  
  return Terminal;
}

static var Table_Iter_Next(var self, var curr) {
  struct Table* t = self;
  
  curr = (char*)curr + Table_Step(t);
  
  while (True) {

    if (curr > Table_Key(t, t->nslots-1)) {
      return Terminal;
    }

    uint64_t h = *(uint64_t*)((char*)curr - sizeof(struct CelloHeader) - sizeof(uint64_t));
    if (h isnt 0) { return curr; }
    
    curr = (char*)curr + Table_Step(t);
  }
  
  return Terminal;
}

static int Table_Show(var self, var output, int pos) {
  struct Table* t = self;
  
  pos = print_to(output, pos, "<'Table' At 0x%p {", self);
  
  size_t j =0;
  for(size_t i = 0; i < t->nslots; i++) {
    if (Table_Key_Hash(t, i) isnt 0) {
      pos = print_to(output, pos, "%$:%$",
        Table_Key(t, i), Table_Val(t, i));
      if (j < Table_Len(t)-1) { pos = print_to(output, pos, ", "); }
      j++;
    }
  }
  
  pos = print_to(output, pos, "}>");
  
  return pos;
}

static void Table_Reserve(var self, var amount) {
  struct Table* t = self;
  int64_t nnslots = c_int(amount);
  
#if CELLO_BOUND_CHECK == 1
  if (nnslots < (int64_t)t->nitems) {
    throw(IndexOutOfBoundsError, 
      "Table already has %li items, cannot reserve %li", $I(t->nitems), amount);
  }
#endif
  
  Table_Rehash(t, Table_Ideal_Size((size_t)nnslots));
}

static var Table_Gen(void) {
  
  var ktype = gen(Type);
  var vtype = gen(Type);
  var t = new(Table, ktype, vtype);
  
  size_t n = gen_c_int() % 10;
  for (size_t i = 0; i < n; i++) {
    var k = gen(ktype);
    var v = gen(vtype);
    set(t, k, v);
    del(k); del(v);
  }
  
  return t;
}

var Table = Cello(Table,
  Member(Doc,
    Table_Name, Table_Brief, Table_Description,
    Table_Examples, Table_Methods),
  Member(Size,    Table_Size),
  Member(New,     Table_New, Table_Del),
  Member(Assign,  Table_Assign),
  Member(Copy,    Table_Copy),
  Member(Eq,      Table_Eq),
  Member(Len,     Table_Len),
  Member(Get,     Table_Get, Table_Set, Table_Mem, Table_Rem),
  Member(Clear,   Table_Clear),
  Member(Iter,    Table_Iter_Init, Table_Iter_Next),
  Member(Show,    Table_Show, NULL),
  Member(Reserve, Table_Reserve),
  Member(Gen,     Table_Gen));

