import torch
import torch.nn as nn

class LeNet5(nn.Module):
    """
    LeNet-5 for CIFAR-10
    """
    def __init__(self, num_classes=10):
        super(LeNet5, self).__init__()
        # TODO
    
    def forward(self, x):
        # TODO
        return 


class ResidualBlock(nn.Module):
    """
    Residual Block for ResNet
    """
    def __init__(self, in_channels, out_channels, stride=1):
        super(ResidualBlock, self).__init__()
        # TODO
        
    def forward(self, x):
        # TODO
        return 

class SimplifiedResNet(nn.Module):
    """
    Simplified ResNet for CIFAR-10
    """
    def __init__(self, num_classes=10, num_blocks=3):
        super(SimplifiedResNet, self).__init__()
        # TODO
    
    def forward(self, x):
        # TODO
        return 
