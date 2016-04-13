#pragma once

#include <memory>
#include <string>
#include <vector>

#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/driver.hpp>
#include <arbiter/drivers/http.hpp>
#endif

namespace arbiter
{

namespace drivers
{

/** @brief AWS authentication information. */
class AwsAuth
{
public:
    AwsAuth(std::string access, std::string hidden);

    /** @brief Search for credentials in some common locations.
     *
     * See:
     * https://docs.aws.amazon.com/AWSJavaScriptSDK/guide/node-configuring.html
     *
     * Uses methods 2 and 3 of "Setting AWS Credentials":
     *      - Check for them in `~/.aws/credentials`.
     *      - If not found, try the environment settings.
     */
    static std::unique_ptr<AwsAuth> find(std::string user = "");

    std::string access() const;
    std::string hidden() const;

private:
    std::string m_access;
    std::string m_hidden;
};

/** @brief Amazon %S3 driver. */
class S3 : public CustomHeaderDriver
{
public:
    S3(HttpPool& pool, AwsAuth awsAuth);

    /** Try to construct an S3 Driver.  Searches @p json primarily for the keys
     * `access` and `hidden` to construct an AwsAuth.  If not found, common
     * filesystem locations and then the environment will be searched (see
     * AwsAuth::find).
     */
    static std::unique_ptr<S3> create(HttpPool& pool, const Json::Value& json);

    virtual std::string type() const override { return "s3"; }

    virtual std::unique_ptr<std::size_t> tryGetSize(
            std::string path) const override;

    virtual void put(
            std::string path,
            const std::vector<char>& data) const override;

    /** Inherited from CustomHeaderDriver. */
    virtual std::string get(std::string path, Headers headers) const override;

    /** Inherited from CustomHeaderDriver. */
    virtual std::vector<char> getBinary(
            std::string path,
            Headers headers) const override;

private:
    virtual bool get(std::string path, std::vector<char>& data) const override;
    virtual std::vector<std::string> glob(
            std::string path,
            bool verbose) const override;

    bool buildRequestAndGet(
            std::string rawPath,
            const Query& query,
            std::vector<char>& data,
            Headers = Headers()) const;

    Headers httpGetHeaders(std::string filePath) const;
    Headers httpPutHeaders(std::string filePath) const;

    std::string getHttpDate() const;

    std::string getSignedEncodedString(
            std::string command,
            std::string file,
            std::string httpDate,
            std::string contentType = "") const;

    std::string getStringToSign(
            std::string command,
            std::string file,
            std::string httpDate,
            std::string contentType) const;

    std::vector<char> signString(std::string input) const;
    std::string encodeBase64(std::vector<char> input) const;

    HttpPool& m_pool;
    AwsAuth m_auth;
};

} // namespace drivers
} // namespace arbiter

