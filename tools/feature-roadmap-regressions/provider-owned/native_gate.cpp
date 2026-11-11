#include "user_content.h"
static_assert(!Eden::UserContent::kIoAvailable);
int main() {
    Eden::UserContent::Descriptor descriptor(-1);
    return descriptor.value != -1;
}
