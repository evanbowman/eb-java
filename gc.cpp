#include "gc.hpp"
#include "memory.hpp"
#include "object.hpp"
#include "returnAddress.hpp"
#include "array.hpp"
#include "vm.hpp"



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



static inline void mark_object(Object* object)
{
    if (object == nullptr) {
        return;
    }

    object->header_.gc_mark_bit_ = 1;

    if (object->class_ == &reference_array_class) {
        auto array = (Array*)object;
        for (int i = 0; i < array->size_; ++i) {
            Object* obj;
            memcpy(&obj,
                   array->data() + i * sizeof(Object*),
                   sizeof(Object*));
            mark_object(obj);
        }
    } else if (object->class_ == &return_address_class or
               object->class_ == &primitive_array_class) {
        // Nothing to do
    } else {
        // TODO: mark fields!!!

    // This is one of the most complicated parts of the whole gc implementation!
    // We need to go back and look at the classfile to determine which of our
    // fields are objects. The rest is straightforward compared to the tedium of
    // fetching info from the classfile again. See link_field(), where compute
    // the offset of an instance's fields...
    //
    // Potential shortcuts:
    // The constant pool already binds substitution fields to cpool
    // indices. Maybe we can somehow leverage that info, to save some effort?
    //
    // We're also dealing with inherited fields from superclasses, so we'll need
    // to walk the chain all the way back up? But a Substitution field already
    // tells us whether it contains an object...
    //
    // For each field in classfile:
    //   Lookup field ref in classfile:
    //     Go to same index in cpool, where substitution field is bound
    //       If it's an object field, fetch and mark from offset in object
    // repeat for all superclasses
    //
    }
}



void mark()
{
    for (u32 i = 0; i < operand_stack().size(); ++i) {
        if (operand_types()[i] == OperandTypeCategory::object) {
            mark_object((Object*)operand_stack()[i]);
        }
    }

    for (u32 i = 0; i < locals().size(); ++i) {
        if (local_types()[i] == OperandTypeCategory::object) {
            mark_object((Object*)locals()[i]);
        }
    }

    // TODO:
    // 1) Mark static vars (fetch them from the class table)
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
