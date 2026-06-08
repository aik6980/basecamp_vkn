#include "scratch_allocator.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

#include "gfx_device/device.h"

namespace VKN {
    namespace {
        static vk::DeviceSize align_up(vk::DeviceSize value, vk::DeviceSize alignment)
        {
            if (alignment <= 1) {
                return value;
            }

            const vk::DeviceSize mask = alignment - 1;
            return (value + mask) & ~mask;
        }
    } // namespace

    Scratch_allocator::Scratch_allocator(Device& gfx_device, vk::DeviceSize default_page_size)
        : m_gfx_device(gfx_device)
        , m_default_page_size(std::max<vk::DeviceSize>(default_page_size, 1024))
    {
    }

    Scratch_allocator::~Scratch_allocator() { destroy(); }

    Scratch_allocator::Page& Scratch_allocator::create_page(vk::DeviceSize min_capacity)
    {
        const vk::DeviceSize capacity = std::max(min_capacity, m_default_page_size);

        auto&& vma_allocator = m_gfx_device.m_vma_allocator;

        vk::BufferCreateInfo buffer_ci{
            .size        = capacity,
            .usage       = vk::BufferUsageFlagBits::eTransferSrc,
            .sharingMode = vk::SharingMode::eExclusive,
        };

        vma::AllocationCreateInfo alloc_ci{};
        alloc_ci.setUsage(vma::MemoryUsage::eAuto);
        alloc_ci.setFlags(
            vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eMapped);

        vk::Buffer buffer{};
        vma::Allocation allocation{};
        vma::AllocationInfo alloc_info{};
        std::tie(allocation, buffer) = vma_allocator.createBuffer(buffer_ci, alloc_ci, alloc_info);

        Page page{};
        page.m_buffer = Buffer{
            .m_buffer     = buffer,
            .m_allocation = allocation,
            .m_size       = static_cast<size_t>(capacity),
        };
        page.m_mapped_ptr = alloc_info.pMappedData;
        page.m_capacity   = capacity;
        page.m_head       = 0;

        m_pages.push_back(page);
        return m_pages.back();
    }

    Scratch_allocator::Allocation Scratch_allocator::allocate(vk::DeviceSize size, vk::DeviceSize alignment)
    {
        if (size == 0) {
            return {};
        }

        if (m_pages.empty()) {
            create_page(size + alignment);
        }

        for (size_t i = 0; i < m_pages.size(); ++i) {
            auto& page = m_pages[i];

            const vk::DeviceSize aligned_head = align_up(page.m_head, alignment);
            if (aligned_head + size > page.m_capacity) {
                continue;
            }

            Scratch_allocator::Allocation out{};
            out.m_buffer     = page.m_buffer.m_buffer;
            out.m_offset     = aligned_head;
            out.m_mapped_ptr = static_cast<uint8_t*>(page.m_mapped_ptr) + aligned_head;
            out.m_size       = size;

            page.m_head = aligned_head + size;
            return out;
        }

        auto& new_page                    = create_page(size + alignment);
        const vk::DeviceSize aligned_head = align_up(new_page.m_head, alignment);

        Scratch_allocator::Allocation out{};
        out.m_buffer     = new_page.m_buffer.m_buffer;
        out.m_offset     = aligned_head;
        out.m_mapped_ptr = static_cast<uint8_t*>(new_page.m_mapped_ptr) + aligned_head;
        out.m_size       = size;

        new_page.m_head = aligned_head + size;
        return out;
    }

    Scratch_allocator::Allocation Scratch_allocator::allocate_and_copy(
        const void* src, vk::DeviceSize size, vk::DeviceSize alignment)
    {
        if (!src || size == 0) {
            return {};
        }

        Allocation out = allocate(size, alignment);
        if (!out.m_mapped_ptr) {
            throw std::runtime_error("Scratch_allocator allocation failed.");
        }

        std::memcpy(out.m_mapped_ptr, src, static_cast<size_t>(size));
        return out;
    }

    void Scratch_allocator::reset()
    {
        for (auto& page : m_pages) {
            page.m_head = 0;
        }
    }

    void Scratch_allocator::destroy()
    {
        for (auto& page : m_pages) {
            if (page.m_buffer.m_buffer) {
                m_gfx_device.m_device.destroyBuffer(page.m_buffer.m_buffer);
            }
            if (page.m_buffer.m_allocation) {
                m_gfx_device.m_vma_allocator.freeMemory(page.m_buffer.m_allocation);
            }
        }
        m_pages.clear();
    }

} // namespace VKN