#include "gc.hpp"
#include "array.hpp"
#include "memory.hpp"
#include "object.hpp"
#include "returnAddress.hpp"
#include "vm.hpp"
#include "classtable.hpp"



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



size_t aligned_instance_size(Object* obj)
{
    // The allocator aligns objects on the heap to the object alignment
    // boundary, so we must do the same when iterating over them. Objects have
    // their fields appended to the end, tightly packed, so not every instance
    // will be a multiple of the Object alignment size. e.g. an object with a
    // char field, followed by an int field, followed by a char field will
    // occupy (sizeof(Object) + 6) bytes.

    return heap::aligned_size(instance_size(obj));
}



Object* heap_next(Object* current, size_t size)
{
    // NOTE: heap_next is also called during heap compaction, after an object
    // has been relocated, so it is never safe to dereference `current`.

    auto ptr = (u8*)current;

    ptr += size;

    if (ptr == heap::end()) {
        return nullptr;
    }

    return (Object*)ptr;
}



// NOTE: Unsafe to call on pseudo-objects, like Array or ReturnAddress.
void visit_object_fields(Object* object, void (*callback)(Object**))
{
    if (object == nullptr) {
        return;
    }

    // We are repurposing the SubstitutionField design to determine the
    // layout of our own fields, and which of those fields are objects. For
    // efficiency, the implementation inserts descriptors, called
    // SubstitutionFields, into the constant pool, which describe the size
    // and the offset of a field within an instance of a class. This speeds
    // up field lookup significantly, but we can also use the same
    // architecture to determine which fields in a class are objects, by
    // asking the constant pool for previously bound substitution fields,
    // and inspecting each field that's declared as local to the class
    // definition (local, as opposed to non-local fields, which are
    // referenced by a classfile, but which do not belong to the class
    // itself).

    auto current_clz = object->class_;

    while (current_clz) {
        auto bindings = current_clz->constants_->bindings();

        for (int i = 0; i < bindings.second; ++i) {
            auto binding = bindings.first[i];

            if (binding.field_.valid_ and binding.field_.local_ and
                binding.field_.object_) {

                Object* field;
                memcpy(&field,
                       object->data() + binding.field_.offset_,
                       sizeof field);

                callback(&field);

                memcpy(object->data() + binding.field_.offset_,
                       &field,
                       sizeof field);
            }
        }
        current_clz = current_clz->super_;
    }
}



static inline void mark_object(Object* object)
{
    if (object == nullptr) {
        return;
    }

    if (object->header_.gc_mark_bit_) {
        return;
    }

    object->header_.gc_mark_bit_ = 1;

    if (object->class_ == &reference_array_class) {
        auto array = (Array*)object;
        for (int i = 0; i < array->size_; ++i) {
            Object* obj;
            memcpy(&obj, array->data() + i * sizeof(Object*), sizeof(Object*));
            mark_object(obj);
        }
    } else if (object->class_ == &return_address_class or
               object->class_ == &primitive_array_class) {
        // Nothing to do
    } else {

        visit_object_fields(object, [](Object** obj) { mark_object(*obj); });
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

    classtable::visit([](Class* clz) {
        auto opts = clz->options_;

        while (opts) {
            if (opts->type_ == Class::Option::Type::static_field) {
                auto field = (Class::OptionStaticField*)opts;
                if (field->is_object_) {
                    Object* static_obj;
                    memcpy(&static_obj, field->data(), sizeof static_obj);
                    mark_object(static_obj);
                }
            }

            opts = opts->next_;
        }
    });
}



void assign_forwarding_pointers()
{
    auto current = (Object*)heap::begin();

    size_t gap = 0;

    while (current) {

        const auto size = aligned_instance_size(current);

        current->header_.gc_forwarding_offset_ =
            (((u8*)current) - gap) - (u8*)heap::begin();

        if (not current->header_.gc_mark_bit_) {
            gap += size;
        }

        current = heap_next(current, size);
    }
}



Object* resolve_forwarding_address(Object* object)
{
    if (object == nullptr) {
        return nullptr;
    }
    if (not object->header_.gc_mark_bit_) {
        return object;
    }
    return (Object*)(heap::begin() + object->header_.gc_forwarding_offset_);
}



void resolve_forwarding_pointers()
{
    // First, fix pointers to objects in local variables, static variables, and
    // operand stack slots.

    // Resolve addresses in operand stack
    for (u32 i = 0; i < operand_stack().size(); ++i) {
        if (operand_types()[i] == OperandTypeCategory::object) {
            operand_stack()[i] =
                resolve_forwarding_address((Object*)operand_stack()[i]);
        }
    }

    // Resolve addresses in local variables
    for (u32 i = 0; i < locals().size(); ++i) {
        if (local_types()[i] == OperandTypeCategory::object) {
            locals()[i] = resolve_forwarding_address((Object*)locals()[i]);
        }
    }

    // Resolve addresses in static variables
    classtable::visit([](Class* clz) {
        auto opts = clz->options_;

        while (opts) {
            if (opts->type_ == Class::Option::Type::static_field) {
                auto field = (Class::OptionStaticField*)opts;
                if (field->is_object_) {
                    Object* static_obj;
                    memcpy(&static_obj, field->data(), sizeof static_obj);
                    static_obj = resolve_forwarding_address(static_obj);
                    memcpy(field->data(), &static_obj, sizeof static_obj);
                }
            }

            opts = opts->next_;
        }
    });

    // Now, scan the heap, and fix internal pointers to other objects...
    {
        auto current = (Object*)heap::begin();
        while (current) {
            const auto size = aligned_instance_size(current);

            if (current->class_ == &reference_array_class) {
                auto array = (Array*)current;
                for (int i = 0; i < array->size_; ++i) {
                    Object* obj;
                    memcpy(&obj,
                           array->data() + i * sizeof(Object*),
                           sizeof(Object*));
                    obj = resolve_forwarding_address(obj);
                    memcpy(array->data() + i * sizeof(Object*),
                           &obj,
                           sizeof(Object*));
                }
            } else if (current->class_ == &return_address_class or
                       current->class_ == &primitive_array_class) {
                // Nothing to do
            } else {
                visit_object_fields(current, [](Object** field) {
                    *field = resolve_forwarding_address(*field);
                });
            }

            current = heap_next(current, size);
        }
    }
}



// Only safe to call when src >= dest. Intended for moving memory from a higher
// address to a lower address, during heap compaction.
void compacting_memmove(u8* dest, u8* src, size_t amount)
{
    if (dest == src) {
        // Nothing to do.
        return;
    }

    const bool is_overlapping = dest + amount >= src;

    if (is_overlapping) {

        while (amount--) {
            *(dest++) = *(src++);
        }
    } else {
        memcpy(dest, src, amount);
    }
}



u32 compact()
{
    auto current = (Object*)heap::begin();

    size_t gap = 0;

    while (current) {

        const auto size = aligned_instance_size(current);

        if (not current->header_.gc_mark_bit_) {
            gap += size;
        } else {
            current->header_.gc_mark_bit_ = 0;
            compacting_memmove(((u8*)current - gap), (u8*)current, size);
        }

        current = heap_next(current, size);
    }

    return gap;
}



u32 collect()
{
    if (heap::begin() == heap::end()) {
        return 0;
    }

    mark();
    assign_forwarding_pointers();
    resolve_forwarding_pointers();
    auto freed_bytes = compact();

    heap::__overwrite_end(heap::end() - freed_bytes);

    if ((size_t)heap::end() % alignof(Object) not_eq 0) {
        unhandled_error("heap corruption");
    }

    return freed_bytes;
}



} // namespace gc
} // namespace jvm
} // namespace java
