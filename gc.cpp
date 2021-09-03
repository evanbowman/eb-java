#include "gc.hpp"
#include "memory.hpp"
#include "object.hpp"
#include "returnAddress.hpp"
#include "array.hpp"



namespace java {
namespace jvm {



// Our implementation includes two classes of pseudo-objects, the Array, and the
// ReturnAddress. These do not contain fields as Java Objects do, and their
// sizes need to be calculated differently.
extern Class primitive_array_class;
extern Class reference_array_class;
extern Class return_address_class;



namespace gc {



size_t instance_size(Object* obj)
{
    if (obj->class_ == &return_address_class) {
        return sizeof(ReturnAddress);
    } else if (obj->class_ == &primitive_array_class or
               obj->class_ == &reference_array_class) {
        return ((Array*)obj)->memory_footprint();
    } else {
        return obj->class_->instance_size();
    }
}



Object* heap_next(Object* current, size_t size)
{
    auto ptr = (u8*)current;

    // The allocator aligns objects on the heap to the object alignment
    // boundary, so we must do the same when iterating over them. Objects have
    // their fields appended to the end, tightly packed, so not every instance
    // will be a multiple of the Object alignment size. e.g. an object with
    // three char fields will occupy (sizeof(Object) + 3) bytes.
    while (size % alignof(Object) not_eq 0) {
        ++size;
    }

    ptr += size;

    if (ptr == heap::end()) {
        return nullptr;
    }

    return (Object*)ptr;
}



void mark()
{
    // TODO:
    // 1) Mark static vars
    // 2) Mark objects on operand stack
    // 3) Mark objects bound to local variables in stack frames
    // ... that's everything, right? ...
}



void assign_forwarding_pointers()
{
    // TODO:
    // Iterate over each object on the heap, as if we were going to run the
    // compaction. Assign each object's forwarding pointer to its future
    // address. Then, in a subsequent interation, we can fix all object pointers
    // by replacing each object reference with its contained forwarding address.
}



void debug_visit_heap()
{
    auto current = (Object*)heap::begin();

    while (current) {

        const auto size = instance_size(current);

        std::cout << "visit object on the heap, mark: "
                  << current->header_.gc_mark_bit_
                  << ", size: "
                  << size
                  << std::endl;
        current = heap_next(current, size);
    }
}



u32 compact()
{
    return 0;
}



u32 collect()
{
    if (heap::begin() == heap::end()) {
        return 0;
    }

    mark();
    assign_forwarding_pointers();
    debug_visit_heap();
    return compact();
}



}
}
}
