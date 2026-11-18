#include "resize.h"
#include <stdexcept>

namespace seam_carver
{
    void resize_graph_cut(Image &image, std::size_t target_width, std::size_t target_height,
                          const Progress &progress, const CancelCheck &cancelled)
    {
        check_cancelled(cancelled);
        validate_image(image);
        if (target_width == 0 || target_height == 0 ||
            target_width > image.width || target_height > image.height)
            throw std::invalid_argument("Target dimensions must be positive and cannot exceed the original.");
        const std::size_t total = image.width - target_width + image.height - target_height;
        std::size_t completed = 0;
        if (progress)
            progress(completed, total);
        while (image.width > target_width)
        {
            const Seam seam = find_vertical_graph_cut(calculate_energy(image), cancelled);
            check_cancelled(cancelled);
            remove_vertical_seam(image, seam.indices);
            if (progress)
                progress(++completed, total);
        }
        while (image.height > target_height)
        {
            const Seam seam = find_horizontal_graph_cut(calculate_energy(image), cancelled);
            check_cancelled(cancelled);
            remove_horizontal_seam(image, seam.indices);
            if (progress)
                progress(++completed, total);
        }
    }
} // namespace seam_carver
