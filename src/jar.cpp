#include "jar.hpp"
#include "classfile.hpp"
#include "endian.hpp"
#include <string.h>



namespace java {
namespace jar {


namespace zip {


struct LocalFileHeader {
    host_u32 signature_;
    host_u16 minimum_version_;
    host_u16 gp_bit_flag_;
    host_u16 compression_method_;
    host_u16 last_modification_time_;
    host_u16 last_modification_date_;
    host_u32 uncompressed_data_crc32_;
    host_u32 compressed_size_;
    host_u32 uncompressed_size_;
    host_u16 file_name_length_;
    host_u16 extra_field_length_;
    // u8 file_name[file_name_length_];
    // u8 extra_field[extra_field_length_];
};


} // namespace zip



Slice load_file_data(const char* jar_file_bytes, Slice path, const char** error)
{


    while (true) {
        auto hdr = (const zip::LocalFileHeader*)jar_file_bytes;

        if (hdr->signature_.get() not_eq 0x04034b50) {
            // Invalid file header
            if (error) {
                *error = "invalid file header";
            }
            return {nullptr, 0};
        }

        if (hdr->compression_method_.get() not_eq 0) {
            // We cannot support compressed files within jars. Doing so would
            // greatly limit our ability to run larger java programs.
            if (error) {
                *error = "JRE.gba does not support jar compression";
            }
            return {nullptr, 0};
        }

        if (hdr->compressed_size_.get() not_eq hdr->uncompressed_size_.get()) {
            // We should never really reach this point, unless the file is
            // corrupt.
            if (error) {
                *error = "file compressed or corrupt";
            }
            return {nullptr, 0};
        }

        if (hdr->gp_bit_flag_.get() & (1 << 3)) {
            // We do not support data descriptors in zip files.
            // TODO: maybe we'll support this someday?
            if (error) {
                *error = "unsupported data descriptor in jar file";
            }
            return {nullptr, 0};
        }

        jar_file_bytes += sizeof(zip::LocalFileHeader);

        const char* file_name_ptr = jar_file_bytes;

        jar_file_bytes += hdr->file_name_length_.get();
        jar_file_bytes += hdr->extra_field_length_.get();

        if (path == Slice{file_name_ptr, hdr->file_name_length_.get()}) {
            return {jar_file_bytes, hdr->compressed_size_.get()};
        } else {
            jar_file_bytes += hdr->compressed_size_.get();
        }
    }

    return {nullptr, 0};
}



Slice load_classfile(const char* jar_file_bytes, Slice classpath, const char** error)
{
    static const int max_classpath = 256;

    if (error) {
        *error = nullptr;
    }


    if (classpath.length_ + 6 > max_classpath) {
        return {nullptr, 0};
    }

    char buffer[max_classpath];
    memset(buffer, 0, max_classpath);
    memcpy(buffer, classpath.ptr_, classpath.length_);
    memcpy(buffer + classpath.length_, ".class", 6);

    auto data = load_file_data(jar_file_bytes, {buffer, classpath.length_ + 6}, error);

    if (error && *error) {
        return Slice{};
    }

    if (data.length_ >= sizeof(ClassFile::HeaderSection1)) {
        if (((ClassFile::HeaderSection1*)data.ptr_)->magic_.get() not_eq
            0xcafebabe) {

            if (error) {
                *error = "classfile invalid magic";
            }

            return Slice{};
        } else {
            return data;
        }
    } else {
        if (error) {
            *error = "jar header invalid";
        }
        return Slice{};
    }
}



} // namespace jar
} // namespace java
