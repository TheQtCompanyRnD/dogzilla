#include <QApplication>

#include <rclcpp/rclcpp.hpp>

#include "turtlesim/turtle_frame.hpp"

class DogzillaApp : public QApplication
{
public:
  rclcpp::Node::SharedPtr nh_;

  explicit DogzillaApp(int & argc, char ** argv)
  : QApplication(argc, argv)
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
    frame.show();

    return QApplication::exec();
  }
};

int main(int argc, char ** argv)
{
  DogzillaApp app(argc, argv);
  return app.exec();
}
