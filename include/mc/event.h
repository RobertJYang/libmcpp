#ifndef MC_EVENT_H
#define MC_EVENT_H

#include <string>
#include <memory>
#include <typeinfo>
#include <typeindex>

namespace mc {

/**
 * @brief 事件基类
 * 
 * 所有事件类型都应该继承自此类
 */
class event {
public:
    /**
     * @brief 构造函数
     */
    event() = default;
    
    /**
     * @brief 虚析构函数
     */
    virtual ~event() = default;
    
    /**
     * @brief 获取事件类型
     * @return 事件类型的type_index
     */
    virtual std::type_index type() const {
        return std::type_index(typeid(*this));
    }
    
    /**
     * @brief 判断事件是否被接受
     * @return 如果事件被接受则返回true，否则返回false
     */
    bool is_accepted() const { return accepted_; }
    
    /**
     * @brief 接受事件
     */
    void accept() { accepted_ = true; }
    
    /**
     * @brief 忽略事件
     */
    void ignore() { accepted_ = false; }
    
private:
    bool accepted_ = false;  ///< 事件是否被接受
};

/**
 * @brief 事件处理器接口
 * 
 * 所有需要处理事件的类都应该实现此接口
 */
class event_handler {
public:
    /**
     * @brief 虚析构函数
     */
    virtual ~event_handler() = default;
    
    /**
     * @brief 事件处理函数
     * @param e 要处理的事件
     * @return 如果事件被处理则返回true，否则返回false
     */
    virtual bool handle_event(event& e) = 0;
};

/**
 * @brief 事件过滤器接口
 * 
 * 事件过滤器可以在事件到达目标对象之前拦截和处理事件
 */
class event_filter {
public:
    /**
     * @brief 虚析构函数
     */
    virtual ~event_filter() = default;
    
    /**
     * @brief 过滤事件
     * @param e 要过滤的事件
     * @param target 事件目标对象
     * @return 如果事件被过滤器处理则返回true，否则返回false
     */
    virtual bool filter_event(event& e, event_handler* target) = 0;
};

} // namespace mc

#endif // MC_EVENT_H