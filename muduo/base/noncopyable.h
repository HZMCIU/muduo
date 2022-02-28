#ifndef MUDUO_BASE_NONCOPYABLE_H
#define MUDUO_BASE_NONCOPYABLE_H

/**
 * @comment noncopyable
 * @link https://blog.csdn.net/weixin_38616705/article/details/121568997
 * @brief noncopyable类中将拷贝构造函数和赋值运算符都指示删除，即不让编译器生成这两个函数（而默认情况下是会自动生成的），
 * 这两个函数删除后，也就禁止了类的拷贝，所有继承了noncopyable的子类都不能被拷贝，protected的作用同上
 */
namespace muduo {

class noncopyable {
public:
    noncopyable(const noncopyable&) = delete;
    /*
     * @question why assign constructor's return value is void
     */
    void operator=(const noncopyable&) = delete;

protected:
    noncopyable() = default;
    ~noncopyable() = default;
};

}  // namespace muduo

#endif  // MUDUO_BASE_NONCOPYABLE_H
