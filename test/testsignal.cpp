#include <iostream>
#include <lkcore/lobject.h>
#include <lkcore/lsignal.h>
#include <string>
#include <lkcore/time/ltimer.h>
#include "lkcore/lcoreapplication.h"
#include "lkcore/ldebug.h"

#if LOOKI_NAMESPACE
using namespace lk;
#endif

using Product = std::string;

class Producer : public LObject {
public:
    Producer() {
        m_timer.setInterval(std::chrono::seconds(1));
        m_timer.sigTimeout.connect(this, &Producer::on_triggered, ConnectionType::Queued);
    }

    LSignal<int>& finished() {
        return m_finished_signal;
    }

    LSignal<Product>& product_available() {
        return m_product_available_signal;
    }

    void produce() {
        m_timer.start();
    }

private:
    void on_triggered() {
        std::cout << "Timer triggerd";

        if (num_products != 5) {
            m_product_available_signal.emt("Product " + std::to_string(num_products++));
            m_timer.start();
        } else {
            m_finished_signal.emt(0);
        }
    }

    LTimer m_timer;
    LSignal<Product> m_product_available_signal;
    LSignal<int> m_finished_signal;
    int num_products = 0;
};

class Consumer : public LObject {
public:
    Consumer(){}

    void consume(Product product) {
        std::cout << "Consuming " << product << std::endl;
    }
};

int main(int argc, char* argv[]) {
    //lDebug() << "test debug";
    std::shared_ptr<LCoreApplication> app = makeInit<LCoreApplication>(argc, argv);

    Consumer consumer;

    Producer producer;
    producer.finished().connect(app.get(), &LCoreApplication::quit);
    producer.product_available().connect(&consumer, &Consumer::consume);

    producer.produce();
    return app->exec();
}


