#pragma once

template<typename T>
class Singleton
{
public:
    Singleton(const Singleton &) = delete;
    Singleton(Singleton &&) = delete;
    Singleton &operator=(const Singleton &) = delete;
    Singleton &operator=(Singleton &&) = delete;

    static T &GetInstance()
    {
        static InstanceT instance;
        return instance;
    }

protected:
    Singleton() = default;
    ~Singleton() = default;

private:
    struct InstanceT : T {};
};
