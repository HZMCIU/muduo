#ifndef MUDUO_BASE_COPYABLE_H
#define MUDUO_BASE_COPYABLE_H

namespace muduo {

/// A tag class emphasises the objects are copyable.
/// The empty base class optimization applies.
/// Any derived class of copyable should be a value type.
/**
 * @comment 1 empty base optimization
 * @link https://stackoverflow.com/questions/4325144/when-do-programmers-use-empty-base-optimization-ebo
 * @brief it's a perfect example of don't pay for what you don't use.
 */
class copyable {
    /**
     * 继承了copyable的子类为值类型，可以拷贝赋值，但是copyable这个类本身不可生成对象。所以将构造析构函数设置为protected。
     */
protected:
    copyable() = default;
    ~copyable() = default;
};

}  // namespace muduo

#endif  // MUDUO_BASE_COPYABLE_H
