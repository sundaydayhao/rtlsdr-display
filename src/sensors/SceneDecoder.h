#pragma once
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <vector>
#include <cstdint>

/**
 * @brief 场景解码器基类
 * 所有具体场景解码器（车钥匙、电表水表、胎压）均继承此类
 */
class SceneDecoder : public QObject
{
    Q_OBJECT
public:
    explicit SceneDecoder(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~SceneDecoder() = default;

    /**
     * @brief 尝试解码一帧原始比特数据
     * @param bits  OOK解调后的比特序列
     * @param freq  接收频率（Hz）
     * @return 解码成功返回 true，并通过 decoded() 信号发出结果
     */
    virtual bool decode(const std::vector<uint8_t> &bits, double freq) = 0;

    virtual QString sceneName() const = 0;

signals:
    /**
     * @brief 解码成功时发出
     * @param result 包含解码字段的键值对
     */
    void decoded(const QVariantMap &result);
};