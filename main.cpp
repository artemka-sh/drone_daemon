#include "pipelineBuilder.h"
#include <gst/gst.h>
#include <future>
static PipelineBuilder builder;

int main(int argc, char** argv)
{
    gst_init(&argc, &argv);
    builder.run();
    std::promise<void>().get_future().wait();
    return 0;
}