#include "pipelineBuilder.h"
#include <signal.h>

static PipelineBuilder builder;

int main()
{
    //регистрация прерывания и блокировка стандартного поведения
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigprocmask(SIG_BLOCK, &set, nullptr);


    builder.run();

    // ctrl + c для остановки
    int sig;
    sigwait(&set, &sig);

    return 0;
}