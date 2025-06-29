#ifndef TRACKING_HPP
#define TRACKING_HPP

#include <cmath>

struct StreamReference {
  size_t index;
  size_t length;
};

class Tracking {
public:
  static StreamReference sum_references(StreamReference ref_from,
                                        StreamReference ref_to) {
    // if parameters are swapped, just switch them around...
    if (ref_from.index > ref_to.index)
      return Tracking::sum_references(ref_to, ref_from);

    return StreamReference{
        ref_from.index,
        (ref_to.index - ref_from.index) + ref_to.length,
    };
  }
};

#endif
