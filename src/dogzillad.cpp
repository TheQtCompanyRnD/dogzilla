#include <QGuiApplication>

#include <rclcpp/rclcpp.hpp>

#include "turtlesim/turtle_frame.hpp"

class DogzillaApp : public QGuiApplication
{
public:
  rclcpp::Node::SharedPtr nh_;

  explicit DogzillaApp(int & argc, char ** argv)
  : QGuiApplication(argc, argv)
  {
    rclcpp::init(argc, argv);
    nh_ = rclcpp::Node::make_shared("turtlesim");
  }

  ~DogzillaApp()
  {
    rclcpp::shutdown();
  }

  int exec()
  {
	  // TODO create OLED framebuffer UI instead

    turtlesim::TurtleFrame frame(nh_);

    return QGuiApplication::exec();
  }
};

int main(int argc, char ** argv)
{
  DogzillaApp app(argc, argv);
  return app.exec();
}
